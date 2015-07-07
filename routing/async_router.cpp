#include "routing/async_router.hpp"

#include "platform/platform.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "indexer/mercator.hpp"

namespace routing
{

namespace
{

string ToString(IRouter::ResultCode code)
{
  switch (code)
  {
  case IRouter::NoError: return "NoError";
  case IRouter::Cancelled: return "Cancelled";
  case IRouter::NoCurrentPosition: return "NoCurrentPosition";
  case IRouter::InconsistentMWMandRoute: return "InconsistentMWMandRoute";
  case IRouter::RouteFileNotExist: return "RouteFileNotExist";
  case IRouter::StartPointNotFound: return "StartPointNotFound";
  case IRouter::EndPointNotFound: return "EndPointNotFound";
  case IRouter::PointsInDifferentMWM: return "PointsInDifferentMWM";
  case IRouter::RouteNotFound: return "RouteNotFound";
  case IRouter::InternalError: return "InternalError";
  case IRouter::NeedMoreMaps: return "NeedMoreMaps";
  }
}

map<string, string> PrepareStatisticsData(string const & routerName,
                                          m2::PointD const & startPoint, m2::PointD const & startDirection,
                                          m2::PointD const & finalPoint)
{
  // Coordinates precision in 5 digits after comma corresponds to metres (0,00001degree ~ 1meter),
  // therefore we round coordinates up to 5 digits after comma.
  int constexpr precision = 5;

  return {{"name", routerName},
          {"startLon", strings::to_string_dac(MercatorBounds::XToLon(startPoint.x), precision)},
          {"startLat", strings::to_string_dac(MercatorBounds::YToLat(startPoint.y), precision)},
          {"startDirectionX", strings::to_string_dac(startDirection.x, precision)},
          {"startDirectionY", strings::to_string_dac(startDirection.y, precision)},
          {"finalLon", strings::to_string_dac(MercatorBounds::XToLon(finalPoint.x), precision)},
          {"finalLat", strings::to_string_dac(MercatorBounds::YToLat(finalPoint.y), precision)}};
}

}  // namespace

AsyncRouter::AsyncRouter(unique_ptr<IRouter> && router, unique_ptr<OnlineAbsentCountriesFetcher> && fetcher,
                         TRoutingStatisticsCallback const & routingStatisticsFn)
    : m_absentFetcher(move(fetcher)),
      m_router(move(router)),
      m_routingStatisticsFn(routingStatisticsFn)
{
  m_isReadyThread.clear();
}

AsyncRouter::~AsyncRouter() { ClearState(); }

void AsyncRouter::CalculateRoute(m2::PointD const & startPoint, m2::PointD const & direction,
                                 m2::PointD const & finalPoint, TReadyCallback const & callback)
{
  ASSERT(m_router, ());
  {
    lock_guard<mutex> guard(m_paramsMutex);
    UNUSED_VALUE(guard);

    m_startPoint = startPoint;
    m_startDirection = direction;
    m_finalPoint = finalPoint;

    m_router->Cancel();
  }

  GetPlatform().RunAsync(bind(&AsyncRouter::CalculateRouteImpl, this, callback));
}

void AsyncRouter::ClearState()
{
  ASSERT(m_router, ());
  m_router->Cancel();

  lock_guard<mutex> guard(m_routeMutex);
  m_router->ClearState();
}

void AsyncRouter::LogCode(IRouter::ResultCode code, double const elapsedSec)
{
  switch (code)
  {
    case IRouter::StartPointNotFound:
      LOG(LWARNING, ("Can't find start or end node"));
      break;
    case IRouter::EndPointNotFound:
      LOG(LWARNING, ("Can't find end point node"));
      break;
    case IRouter::PointsInDifferentMWM:
      LOG(LWARNING, ("Points are in different MWMs"));
      break;
    case IRouter::RouteNotFound:
      LOG(LWARNING, ("Route not found"));
      break;
    case IRouter::RouteFileNotExist:
      LOG(LWARNING, ("There is no routing file"));
      break;
    case IRouter::NeedMoreMaps:
      LOG(LINFO,
          ("Routing can find a better way with additional maps, elapsed seconds:", elapsedSec));
      break;
    case IRouter::Cancelled:
      LOG(LINFO, ("Route calculation cancelled, elapsed seconds:", elapsedSec));
      break;
    case IRouter::NoError:
      LOG(LINFO, ("Route found, elapsed seconds:", elapsedSec));
      break;
    case IRouter::NoCurrentPosition:
      LOG(LINFO, ("No current position"));
      break;
    case IRouter::InconsistentMWMandRoute:
      LOG(LINFO, ("Inconsistent mwm and route"));
      break;
    case IRouter::InternalError:
      LOG(LINFO, ("Internal error"));
      break;
  }
}

void AsyncRouter::CalculateRouteImpl(TReadyCallback const & callback)
{
  if (m_isReadyThread.test_and_set())
    return;

  Route route(m_router->GetName());
  IRouter::ResultCode code;

  lock_guard<mutex> guard(m_routeMutex);

  m_isReadyThread.clear();

  m2::PointD startPoint, finalPoint, startDirection;
  {
    lock_guard<mutex> params(m_paramsMutex);

    startPoint = m_startPoint;
    finalPoint = m_finalPoint;
    startDirection = m_startDirection;

    m_router->Reset();
  }

  my::Timer timer;
  try
  {
    LOG(LDEBUG, ("Calculating the route from", startPoint, "to", finalPoint, "startDirection", startDirection));

    if (m_absentFetcher)
      m_absentFetcher->GenerateRequest(startPoint, finalPoint);

    // Run basic request.
    code = m_router->CalculateRoute(startPoint, startDirection, finalPoint, route);

    double const elapsedSec = timer.ElapsedSeconds();

    LogCode(code, elapsedSec);

    SendStatistics(startPoint, startDirection, finalPoint, code, elapsedSec);
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Exception happened while calculating route:", e.Msg()));
    code = IRouter::InternalError;

    SendStatistics(startPoint, startDirection, finalPoint, e.Msg());
  }

  if (code == IRouter::NoError)
    GetPlatform().RunOnGuiThread(bind(callback, route, code));

  // Check online response if we have.
  vector<string> absent;
  if (m_absentFetcher)
    m_absentFetcher->GetAbsentCountries(absent);
  if (absent.empty())
  {
    if (code != IRouter::NoError)
      GetPlatform().RunOnGuiThread(bind(callback, route, code));
    return;
  }

  for (string const & country : absent)
    route.AddAbsentCountry(country);
  if (code != IRouter::NoError)
  {
    GetPlatform().RunOnGuiThread(bind(callback, route, code));
  }
  else
  {
    double const elapsedSec = timer.ElapsedSeconds();
    LogCode(IRouter::NeedMoreMaps, elapsedSec);
    SendStatistics(startPoint, startDirection, finalPoint, IRouter::NeedMoreMaps, elapsedSec);
    GetPlatform().RunOnGuiThread(bind(callback, route, IRouter::NeedMoreMaps));
  }
}

void AsyncRouter::SendStatistics(m2::PointD const & startPoint, m2::PointD const & startDirection,
                                 m2::PointD const & finalPoint,
                                 IRouter::ResultCode resultCode,
                                 double elapsedSec)
{
  if (nullptr == m_routingStatisticsFn)
    return;

  map<string, string> statistics = PrepareStatisticsData(m_router->GetName(), startPoint, startDirection, finalPoint);
  statistics.emplace("result", ToString(resultCode));
  statistics.emplace("elapsed", strings::to_string(elapsedSec));

  m_routingStatisticsFn(statistics);
}

void AsyncRouter::SendStatistics(m2::PointD const & startPoint, m2::PointD const & startDirection,
                                 m2::PointD const & finalPoint,
                                 string const & exceptionMessage)
{
  if (nullptr == m_routingStatisticsFn)
    return;

  map<string, string> statistics = PrepareStatisticsData(m_router->GetName(), startPoint, startDirection, finalPoint);
  statistics.emplace("exception", exceptionMessage);

  m_routingStatisticsFn(statistics);
}

}  // namespace routing

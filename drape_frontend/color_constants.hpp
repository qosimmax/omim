#pragma once

#include "drape/color.hpp"

#include "indexer/map_style.hpp"

namespace df
{

enum ColorConstant
{
  GuiText,
  MyPositionAccuracy,
  Selection,
  Route,
  RouteOutline,
  RouteTrafficG0,
  RouteTrafficG1,
  RouteTrafficG2,
  RouteTrafficG3,
  RoutePedestrian,
  RouteBicycle,
  Arrow3D,
  Arrow3DObsolete,
  Arrow3DShadow,
  Arrow3DOutline,
  TrackHumanSpeed,
  TrackCarSpeed,
  TrackPlaneSpeed,
  TrackUnknownDistance,
  TrafficG0,
  TrafficG1,
  TrafficG2,
  TrafficG3,
  TrafficG4,
  TrafficG5,
  TrafficTempBlock,
  TrafficUnknown,
  TrafficArrowLight,
  TrafficArrowDark,
  TrafficOutline,
  RoadShieldAddText,
  RoadShieldAddTextOutline,
  RoadShieldUSInterstateText,
  RoadShieldUSHighwayText,
  RoadShieldBlueText,
  RoadShieldGreenText,
  RoadShieldUKGreenText,
  RoadShieldBlueBackground,
  RoadShieldGreenBackground,
  PoiHotelTextOutline,
  PoiDeletedMask
};

dp::Color GetColorConstant(MapStyle style, ColorConstant constant);

} // namespace df

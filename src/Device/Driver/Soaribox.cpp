// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Device/Driver/Soaribox.hpp"
#include "Device/Driver.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/InputLine.hpp"
#include "NMEA/Checksum.hpp"
#include "Units/System.hpp"
#include "util/StringAPI.hxx"
#include "Language/Language.hpp"
#include "Message.hpp"

using std::string_view_literals::operator""sv;

class SoariboxDevice : public AbstractDevice {
public:
  /* virtual methods from class Device */
  bool ParseNMEA(const char *line, struct NMEAInfo &info) override;
};


static bool
PZAN2(NMEAInputLine &line, NMEAInfo &info)
{
  double vtas, wnet;

  if (line.ReadChecked(vtas))
    info.ProvideTrueAirspeed(Units::ToSysUnit(vtas, Unit::KILOMETER_PER_HOUR));

  if (line.ReadChecked(wnet))
    info.ProvideTotalEnergyVario((wnet - 10000) / 100.);

  return true;
}

static bool
PZAN3(NMEAInputLine &line, NMEAInfo &info)
{
  // old: $PZAN3,+,026,V,321,035,A,321,035,V*cc
  // new: $PZAN3,+,026,A,321,035,V[,A]*cc

  line.Skip(3);

  int direction, speed;
  if (!line.ReadChecked(direction) || !line.ReadChecked(speed))
    return false;

  char okay = line.ReadFirstChar();
  if (okay == 'V') {
    okay = line.ReadFirstChar();
    if (okay == 'V')
      return true;

    if (okay != 'A') {
      line.Skip();
      okay = line.ReadFirstChar();
    }
  }

  if (okay == 'A') {
    SpeedVector wind(Angle::Degrees(direction),
                     Units::ToSysUnit(speed, Unit::KILOMETER_PER_HOUR));
    info.ProvideExternalWind(wind);
  }

  return true;
}

static bool
PZAN4(NMEAInputLine &line, NMEAInfo &info)
{
  // $PZAN4,1.5,+,20,39,45*cc

  double mc;
  if (line.ReadChecked(mc))
    info.settings.ProvideMacCready(mc, info.clock);

  return true;
}

static bool
PZAN5(NMEAInputLine &line, NMEAInfo &info)
{
  // $PZAN5,VA,MUEHL,123.4,KM,T,234*cc

  const auto state = line.ReadView();

  if (state == "SF"sv)
    info.switch_state.flight_mode = SwitchState::FlightMode::CRUISE;
  else if (state == "VA"sv)
    info.switch_state.flight_mode = SwitchState::FlightMode::CIRCLING;
  else
    info.switch_state.flight_mode = SwitchState::FlightMode::UNKNOWN;

  return true;
}

bool
SoariboxDevice::ParseNMEA(const char *String, NMEAInfo &info)
{
  if (!VerifyNMEAChecksum(String))
    return false;

  NMEAInputLine line(String);

  const auto type = line.ReadView();

  if (type == "$SOARIM"sv){
    // return SOARIM(line);
    const auto message = line.Rest();
    StaticString<256> buffer;
    buffer.SetASCII(message);
    Message::AddMessage(buffer);
    return true;
  }

  else if (type == "$PZAN2"sv)
    return PZAN2(line, info);

  else if (type == "$PZAN3"sv)
    return PZAN3(line, info);

  else if (type == "$PZAN4"sv)
    return PZAN4(line, info);

  else if (type == "$PZAN5"sv)
    return PZAN5(line, info);

  else
    return false;
}

static Device *
SoariboxCreateOnPort([[maybe_unused]] const DeviceConfig &config, [[maybe_unused]] Port &com_port)
{
  return new SoariboxDevice();
}

const struct DeviceRegister soaribox_driver = {
  _T("Soaribox"),
  _T("Soaribox"),
  DeviceRegister::RECEIVE_SETTINGS,
  SoariboxCreateOnPort,
};

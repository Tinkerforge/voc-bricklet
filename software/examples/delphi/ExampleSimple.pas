program ExampleSimple;

{$ifdef MSWINDOWS}{$apptype CONSOLE}{$endif}
{$ifdef FPC}{$mode OBJFPC}{$H+}{$endif}

uses
  SysUtils, IPConnection, BrickletAirQuality;

type
  TExample = class
  private
    ipcon: TIPConnection;
    aq: TBrickletAirQuality;
  public
    procedure Execute;
  end;

const
  HOST = 'localhost';
  PORT = 4223;
  UID = 'XYZ'; { Change XYZ to the UID of your Air Quality Bricklet }

var
  e: TExample;

procedure TExample.Execute;
var iaqIndex, temperature, humidity, airPressure: longint; iaqIndexAccuracy: byte;
begin
  { Create IP connection }
  ipcon := TIPConnection.Create;

  { Create device object }
  aq := TBrickletAirQuality.Create(UID, ipcon);

  { Connect to brickd }
  ipcon.Connect(HOST, PORT);
  { Don't use device before ipcon is connected }

  { Get current all values }
  aq.GetAllValues(iaqIndex, iaqIndexAccuracy, temperature, humidity, airPressure);

  WriteLn(Format('IAQ Index: %d', [iaqIndex]));

  if (iaqIndexAccuracy = BRICKLET_AIR_QUALITY_ACCURACY_UNRELIABLE) then begin
    WriteLn('IAQ Index Accuracy: Unreliable');
  end
  else if (iaqIndexAccuracy = BRICKLET_AIR_QUALITY_ACCURACY_LOW) then begin
    WriteLn('IAQ Index Accuracy: Low');
  end
  else if (iaqIndexAccuracy = BRICKLET_AIR_QUALITY_ACCURACY_MEDIUM) then begin
    WriteLn('IAQ Index Accuracy: Medium');
  end
  else if (iaqIndexAccuracy = BRICKLET_AIR_QUALITY_ACCURACY_HIGH) then begin
    WriteLn('IAQ Index Accuracy: High');
  end;

  WriteLn(Format('Temperature: %f °C', [temperature/100.0]));
  WriteLn(Format('Humidity: %f %%RH', [humidity/100.0]));
  WriteLn(Format('Air Pressure: %f hPa', [airPressure/100.0]));

  WriteLn('Press key to exit');
  ReadLn;
  ipcon.Destroy; { Calls ipcon.Disconnect internally }
end;

begin
  e := TExample.Create;
  e.Execute;
  e.Destroy;
end.

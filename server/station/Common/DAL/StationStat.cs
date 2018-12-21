using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Common.DAL
{
    public class StationStat
    {
        public float ChargedVoltage { get; set; } = 0;

        public float ExternalTempMinC { get; set; } = 0;

        public float ExternalTempMaxC { get; set; } = 0;

        public float ExternalPressureMinPa { get; set; } = 0;

        public float ExternalPressureMaxPa { get; set; } = 0;

        public float BatteryMinV { get; set; } = 0;

        public float BatteryMaxV { get; set; } = 0;

        public float LightMinRAW { get; set; } = 0;

        public float LightMaxRAW { get; set; } = 0;

        public float BoxHumidityMinPERC { get; set; } = 0;

        public float BoxHumidityMaxPERC { get; set; } = 0;

        public float BoxTempMinC { get; set; } = 0;

        public float BoxTempMaxC { get; set; } = 0;
    }
}

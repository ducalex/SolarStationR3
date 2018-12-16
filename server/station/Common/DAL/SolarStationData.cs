using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Common.DAL
{
    public class SolarStationData
    {
        public int id { get; set; }

        public DateTime datetime { get; set; }

        public float? lightsensorRAW {get;set; }

        public float? batteryV { get; set; }

        public float? boxtempC { get; set; }

        public float? boxhumidityPERC { get; set; }

        public int powermode { get; set; }
    }
}

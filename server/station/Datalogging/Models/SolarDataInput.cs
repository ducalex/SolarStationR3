using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Datalogging.Controllers
{
    public class SolarDataInput
    {
        public long offsetms { get; set; } = 0;

        public string s { get; set; } = "";

        public double? battv { get; set; }

        public double? wc { get; set; }

        public double? boxtemp { get; set; }

        public double? boxhumidity { get; set; }

        public double? exttempC { get; set; }

        public double? pressurepa { get; set; }

        public double? light { get; set; }

        public int powersave { get; set; }
    }
}

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Viewer.Hubs
{
    public interface IMainHub_ToClient
    {
        void eventNewSensorData();
    }
}

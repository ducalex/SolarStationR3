using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNet.SignalR;
using Quartz;
using SolarStationServer.Viewer.Hubs;

namespace SolarStationServer.Viewer.Jobs
{
    public class MainJob : IJob
    {
        public async Task Execute(IJobExecutionContext context)
        {
            var hubContext = GlobalHost.ConnectionManager.GetHubContext<MainHub>();
            hubContext.Clients.All.eventNewSensorData();
        }
    }
}

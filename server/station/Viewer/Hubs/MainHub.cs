using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNet.SignalR;
using Microsoft.AspNet.SignalR.Hubs;
using MySql.Data.MySqlClient;
using Newtonsoft.Json;
using SolarStationServer.Common.DAL;

namespace SolarStationServer.Viewer.Hubs
{
    [HubName("mainHub")]
    public class MainHub : Hub
    {
        MySQLDal m_mySQLDal = new MySQLDal();

        public dynamic getStationData(DateTime start, DateTime end)
        {
            return new
            {
                Datas = from data in m_mySQLDal.getAllSolarStationDatas()
                        select data
            };
        }
    }
}

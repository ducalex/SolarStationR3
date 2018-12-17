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
using SolarStationServer.Viewer.Helpers;

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
                        where data.datetime >= start.Date && data.datetime < end.Date.AddDays(1)
                        group data by new
                        {
                            QuarterText = FormatUtil.formatDateYMDQuarter(data.datetime)
                        }
                        into g
                        select new
                        {
                            Xvalue = g.Key.QuarterText,

                            //data.datetime,

                            batteryV = g.Average(d => d.batteryV),
                            lightsensorRAW = g.Average(d => d.lightsensorRAW),

                            boxtempC = g.Average(d => d.boxtempC),
                            boxhumidityPERC = g.Average(d => d.boxhumidityPERC),
                            powermode = g.Max(d => d.powermode),
                        }
            };
        }
    }
}

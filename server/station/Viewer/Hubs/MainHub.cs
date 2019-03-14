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
    public class MainHub : Hub<IMainHub_ToClient>
    {
        MySQLDal m_mySQLDal = new MySQLDal();

        public dynamic getStationStat()
        {
            StationStat ss = m_mySQLDal.getVoltageStatOrDefault();

            return ss;
        }

        public dynamic getStationData(DateTime startUTC, DateTime endUTC)
        {
            DateTime dateD0 = startUTC.ToLocalTime().Date;
            DateTime dateD1 = endUTC.ToLocalTime().Date.AddDays(1);

            dynamic ret = new
            {
                Datas = (from data in m_mySQLDal.getAllSolarStationDatas(dateD0, dateD1)
                        //where data.datetime >= startUTC.ToLocalTime().Date && data.datetime < endUTC.ToLocalTime().Date.AddDays(1)
                        orderby data.datetime
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

                            exttempC = g.Average(d => d.exttempC),
                            extpressureKPA = g.Average(d => d.extpressurePA) / 1000.0d,

                            powermode = g.Max(d => d.powermode),
                        }).ToArray()
            }; ;

            return ret;
        }

        public dynamic getStationData2Day()
        {
            DateTime dateD0 = DateTime.Now.Date.AddDays(-1);
            DateTime dateD1 = DateTime.Now.Date.AddDays(1);

            var allDatas = from data in m_mySQLDal.getAllSolarStationDatas(dateD0, dateD1)
                           orderby data.datetime
                           select data;

            dynamic ret = new
            {
                DayD0Text = dateD0.Date.ToShortDateString(),
                DayD1Text = DateTime.Now.Date.ToShortDateString(),

                Datas =
                    (from hq in FormatUtil.getHourQuarters()
                     let allDataD0s = allDatas.Where(p => p.datetime.Date == dateD0.Date && (FormatUtil.HourQuarter)p.datetime == hq)
                     let allDataD1s = allDatas.Where(p => p.datetime.Date == DateTime.Now.Date && (FormatUtil.HourQuarter)p.datetime == hq)
                     select new
                    {
                        XValue = FormatUtil.formatQuarterHour(hq),

                        DataD0 = new
                        {
                            batteryV = allDataD0s.Where(p => p.batteryV.HasValue).Average(p => p.batteryV),
                            lightsensorRAW = allDataD0s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.lightsensorRAW),
                            exttempC = allDataD0s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.exttempC),
                            extpressureKPA = allDataD0s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.extpressurePA) / 1000.0d,
                        },
                        DataD1 = new {
                            batteryV = allDataD1s.Where(p => p.batteryV.HasValue).Average(p => p.batteryV),
                            lightsensorRAW = allDataD1s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.lightsensorRAW),
                            exttempC = allDataD1s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.exttempC),
                            extpressureKPA = allDataD1s.Where(p => p.lightsensorRAW.HasValue).Average(p => p.extpressurePA) / 1000.0d,
                        },
                     }).ToArray()
            };


            return ret;
        }
    }
}

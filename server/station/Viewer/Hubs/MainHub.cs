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

        //public dynamic getStationStat()
        //{
        //    return new
        //    {
        //        ChargedVoltage = m_mySQLDal.getVoltageStatOrDefault().ChargedVoltage
        //    };
        //}

        public dynamic getStationData(DateTime startUTC, DateTime endUTC)
        {
            StationStat ss = m_mySQLDal.getVoltageStatOrDefault();

            dynamic ret = new
            {
                Datas = (from data in m_mySQLDal.getAllSolarStationDatas()
                        where data.datetime >= startUTC.ToLocalTime().Date && data.datetime < endUTC.ToLocalTime().Date.AddDays(1)
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
                            batteryChargedV = ss.ChargedVoltage,

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

            var allDatas = from data in m_mySQLDal.getAllSolarStationDatas()
                           orderby data.datetime
                           where data.datetime >= dateD0 && data.datetime < dateD1
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
                //Datas = from data in m_mySQLDal.getAllSolarStationDatas()
                //        orderby data.datetime
                //        group data by new
                //        {
                //            QuarterText = FormatUtil.formatDateYMDQuarter(data.datetime)
                //        }
                //        into g
                //        select new
                //        {
                //            Xvalue = g.Key.QuarterText,


                        //        }
            };


            return ret;
        }
    }
}

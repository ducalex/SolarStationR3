using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using SolarStationServer.Common.DAL;

namespace SolarStationServer.Viewer
{
    class TEST_Import
    {
        public static void patate()
        {
            MySQLDal m_mySQLDal = new MySQLDal();

            using(StreamReader sr = new StreamReader(@"C:\Users\x_men\Desktop\GIT\solarweatherstation\server\station\Datalogging\archive\2018-12-16.txt"))
            {
                int linecount = sr.ReadToEnd().Split('\n').Length;

                double timeMin = (1140.0d / linecount);

                DateTime startCursor = new DateTime(2018, 12, 16, 0, 0, 0, 0);

                string line = null;
                sr.BaseStream.Seek(0, SeekOrigin.Begin);

                while ((line = sr.ReadLine()) != null)
                {
                    Regex regexValues = new Regex(@"(?<varname>[\d\w]+)=(?<varvalue>[\d.\w]+)", RegexOptions.ExplicitCapture | RegexOptions.Compiled);
                    MatchCollection mc = regexValues.Matches(line);
                    if (mc.Count == 0)
                        continue;

                    string getVar(string varname)
                    {
                        for (int i = 0; i < mc.Count; i++)
                        {
                            if (mc[i].Groups["varname"].Value == varname)
                                return mc[i].Groups["varvalue"].Value;
                        }

                        return "";
                    }

                    SolarStationData ssd = new SolarStationData()
                    {
                        datetime = startCursor,
                        batteryV = Convert.ToSingle(getVar("battv")),
                        boxhumidityPERC = Convert.ToSingle(getVar("boxhumidity")),
                        boxtempC = Convert.ToSingle(getVar("boxtemp")),
                        lightsensorRAW = Convert.ToSingle(getVar("light")),
                        powermode = Convert.ToInt32(getVar("powersave")),
                    };

                    startCursor = startCursor.AddMinutes(timeMin);

                    m_mySQLDal.insertNewSolarStationData(ssd);
                }
            }
        }
    }
}

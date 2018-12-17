using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Viewer.Helpers
{
    static class FormatUtil
    {
        public static string formatDateYMDHHMMText(DateTime dt)
        {
            if (dt.Date == DateTime.Now.Date)
                return String.Format("{0:00}:{1:00}", /*0*/dt.Hour, /*1*/dt.Minute);

            return $"{formatDateYMDText(dt)} { String.Format("{0:00}:{1:00}", /*0*/dt.Hour, /*1*/dt.Minute) }";
        }


        public static string formatDateYMDText(DateTime dt)
        {
            if (dt.Year == DateTime.Now.Year)
                return String.Format("{0:00}-{1:00}", /*0*/dt.Month, /*1*/dt.Day);

            return String.Format("{0:0000}-{1:00}-{2:00}", /*0*/dt.Year, /*1*/dt.Month, /*2*/dt.Day);
        }

        public static string formatDateYMD(DateTime dt) => String.Format("{0:0000}-{1:00}-{2:00}", /*0*/dt.Year, /*1*/dt.Month, /*2*/dt.Day);
    }
}

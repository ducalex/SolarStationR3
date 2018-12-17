using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SolarStationServer.Viewer.Helpers
{
    static class FormatUtil
    {
        public enum EQuarterGroup
        {
            _0_15 = 0,
            _15_30 = 1,
            _30_45 = 2,
            _45_0 = 3,
        }

        public static EQuarterGroup getQuarterGroup(DateTime dt)
        {
            if (dt.Minute >= 0 && dt.Minute < 15)
                return EQuarterGroup._0_15;
            if (dt.Minute >= 15 && dt.Minute < 30)
                return EQuarterGroup._0_15;
            if (dt.Minute >= 30 && dt.Minute < 45)
                return EQuarterGroup._0_15;
            return EQuarterGroup._45_0;
        }

        public static string formatQuarterHour(DateTime dt)
        {
            switch(getQuarterGroup(dt))
            {
                case EQuarterGroup._0_15:
                    return String.Format("{0:00}:{1:00}-{2:00}:{3:00}", /*0*/dt.Hour, /*1*/0, /*2*/dt.Hour, /*3*/15);
                case EQuarterGroup._15_30:
                    return String.Format("{0:00}:{1:00}-{2:00}:{3:00}", /*0*/dt.Hour, /*1*/15, /*2*/dt.Hour, /*3*/30);
                case EQuarterGroup._30_45:
                    return String.Format("{0:00}:{1:00}-{2:00}:{3:00}", /*0*/dt.Hour, /*1*/30, /*2*/dt.Hour, /*3*/45);
                case EQuarterGroup._45_0:
                    return String.Format("{0:00}:{1:00}-{2:00}:{3:00}", /*0*/dt.Hour, /*1*/45, /*2*/dt.Hour, /*3*/59);
            }
            return "";
        }

        public static string formatDateYMDQuarter(DateTime dt)
        {
            if (dt.Date == DateTime.Now.Date)
                return formatQuarterHour(dt);

            return $"{formatDateYMDText(dt)} {formatQuarterHour(dt)}";
        }

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

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web.Http;
using System.Web.Http.Filters;
using SolarStationServer.Common.DAL;
using SolarStationServer.Datalogging.Filters;

namespace SolarStationServer.Datalogging.Controllers
{
    [ExceptionFilter()]
    public class MainController : ApiController
    {
        MySQLDal m_mySQLDal = new MySQLDal();

        [HttpPost()]
        public IHttpActionResult PostData(SolarDataInput solarDataInput)
        {
            m_mySQLDal.insertNewSolarStationData(new SolarStationData()
            {
                datetime = DateTime.Now.AddMilliseconds(-1 * solarDataInput.offsetms),
                batteryV = (float?)solarDataInput.battv,
                lightsensorRAW = (float?)solarDataInput.light,
                powermode = solarDataInput.powersave,
                boxhumidityPERC = (float?)solarDataInput.boxhumidity,
                boxtempC = (float?)solarDataInput.boxtemp,
                exttempC = (float?)solarDataInput.exttempC,
                extpressurePA = (float?)solarDataInput.pressurepa,
            });
            return Ok(new { Status = "OK" });
        }
    }
}

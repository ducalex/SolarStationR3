using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web.Http;
using System.Web.Http.Filters;
using SolarStationServer.Viewer.Filters;

namespace SolarStationServer.Viewer.Controllers
{
    [ExceptionFilter()]
    public class MainController : ApiController
    {
        [HttpGet()]
        public IHttpActionResult Patate()
        {
            //throw new Exception("aaaa");

            return Ok(new { Patate = "aaa" } );
        }
    }
}

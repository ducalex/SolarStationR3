using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Web.Http.Filters;

namespace SolarStationServer.Datalogging.Filters
{
    public class ExceptionFilter : ExceptionFilterAttribute
    {
        public override void OnException(HttpActionExecutedContext context)
        {
            if (context.Exception is NotImplementedException)
            {
                context.Response = new HttpResponseMessage(HttpStatusCode.NotImplemented);
            }
            else
            {
                context.Response = new HttpResponseMessage(HttpStatusCode.BadRequest)
                {
                    ReasonPhrase = context.Exception.Message
                };
            }
        }
    }
}

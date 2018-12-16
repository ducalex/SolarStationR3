﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web.Http;
using SolarStationServer.Viewer.Filters;
using Owin;
using Microsoft.AspNet.SignalR;
using Microsoft.Owin.Hosting;
using Microsoft.Owin.Cors;
using Microsoft.Owin.StaticFiles;
using Microsoft.Owin.FileSystems;

namespace SolarStationServer.Viewer
{
    public class Startup
    {
        // This code configures Web API. The Startup class is specified as a type
        // parameter in the WebApp.Start method.
        public void Configuration(IAppBuilder appBuilder)
        {
            // Configure Web API for self-host.
            HttpConfiguration config = new HttpConfiguration();
            config.Routes.MapHttpRoute(
                name: "DefaultApi",
                routeTemplate: "api/{controller}/{id}",
                defaults: new { id = RouteParameter.Optional }
            );
            config.Filters.Add(new ExceptionFilter());

            #if DEBUG
            string path = @"..\..";
            #else
            string path= "";
            #endif

            appBuilder.UseFileServer(new FileServerOptions()
            {
                EnableDefaultFiles = true,
                EnableDirectoryBrowsing = true,
                RequestPath = new Microsoft.Owin.PathString(""),
                FileSystem = new PhysicalFileSystem(path)
            });
            appBuilder.UseCors(CorsOptions.AllowAll);
            //appBuilder.MapSignalR();
            appBuilder.MapSignalR("/signalr", new HubConfiguration()
            {

            });
            appBuilder.UseWebApi(config);
        }
    }
}
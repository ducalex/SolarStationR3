using System;
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
using SolarStationServer.Viewer.Jobs;
using Quartz;
using Quartz.Impl;

namespace SolarStationServer.Viewer
{
    public class Startup
    {
        // This code configures Web API. The Startup class is specified as a type
        // parameter in the WebApp.Start method.
        public async void Configuration(IAppBuilder appBuilder)
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

            var options = new FileServerOptions()
            {
                EnableDefaultFiles = true,
                EnableDirectoryBrowsing = false,
                RequestPath = new Microsoft.Owin.PathString(""),
                FileSystem = new PhysicalFileSystem(path),
            };

            appBuilder.UseFileServer(options);
            options.DefaultFilesOptions.DefaultFileNames = new[] { "Default.html" };
            appBuilder.UseCors(CorsOptions.AllowAll);
            //appBuilder.MapSignalR();
            appBuilder.MapSignalR("/signalr", new HubConfiguration()
            {

            });
            appBuilder.UseWebApi(config);

            // construct a scheduler factory
            ISchedulerFactory schedFact = new StdSchedulerFactory();

            // get a scheduler
            IScheduler sched = await schedFact.GetScheduler();
            sched.Start();

            // define the job and tie it to our HelloJob class
            IJobDetail job = JobBuilder.Create<MainJob>()
                .WithIdentity("myJob", "group1") // name "myJob", group "group1"
                .Build();

            // Trigger the job to run now, and then every 40 seconds
            ITrigger trigger = TriggerBuilder.Create()
                .WithIdentity("myTrigger", "group1")
                .StartNow()
                .WithSimpleSchedule(x => x
                    .WithIntervalInSeconds(120)
                    .RepeatForever())
                .Build();

            sched.ScheduleJob(job, trigger);
        }
    }
}

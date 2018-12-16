using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using SolarStationServer.Viewer.Hubs;
using Microsoft.Owin.Hosting;

namespace SolarStationServer.Viewer
{
    class Program
    {
        static void Main(string[] args)
        {
            //MainHub h = new MainHub();
            //h.getCurrentSession(out var a, out var b);

            string baseAddress = "http://+:9999";

            // Start OWIN host
            using (WebApp.Start<Startup>(url: baseAddress))
            {
                Console.ReadLine();
            }
        }
    }
}

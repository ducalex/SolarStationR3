using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Owin.Hosting;
using SolarStationServer.Viewer;

namespace datalogging
{
    class Program
    {
        static void Main(string[] args)
        {
            string baseAddress = "http://+:80";

            // Start OWIN host
            using (WebApp.Start<Startup>(url: baseAddress))
            {
                Console.ReadLine();
            }
        }
    }
}

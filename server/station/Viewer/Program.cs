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
            string baseAddress = "http://+:9999";

            try
            {
                Console.WriteLine($"SolarStation Server Viewer is running (address: '{baseAddress}') ...");

                // Start OWIN host
                using (WebApp.Start<Startup>(url: baseAddress))
                {
                    string line = null;
                    while ((line = Console.ReadLine()) != null)
                    {
                        if (line == "q")
                            break;
                    }
                }

                Console.Out.WriteLine("Gracefully stop ...");
            }
            catch (Exception ex)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.Error.WriteLine("Error: " + ex.Message);
            }
        }
    }
}

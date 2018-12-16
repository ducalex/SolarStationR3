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
            string baseAddress = "http://+:51248";

            try
            {
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

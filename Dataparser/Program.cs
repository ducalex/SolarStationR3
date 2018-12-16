using System;
using System.Collections.Generic;
using System.Dynamic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace Dataparser
{
    class Program
    {
        static void Main(string[] args)
        {
            Regex regexValues = new Regex(@"(?<varname>[\d\w]+)=(?<varvalue>[\d.\w]+)", RegexOptions.ExplicitCapture | RegexOptions.Compiled);

            string line = null;
            while((line = Console.In.ReadLine()) != null)
            {
                JObject jobj = new JObject();
                //jobj.Add()
                MatchCollection matches = regexValues.Matches(line);
                if (matches.Count == 0)
                    continue;

                for (int i = 0; i < matches.Count; i++)
                {
                    Match m = matches[i];

                    string varname = Convert.ToString(m.Groups["varname"].Value);
                    string varvalue = Convert.ToString(m.Groups["varvalue"].Value);
                    if (string.IsNullOrEmpty(varname) || String.IsNullOrEmpty(varvalue))
                        continue;

                    jobj.Add(varname, varvalue);
                }

                Console.Out.WriteLine(jobj.ToString());
            }
        }
    }
}

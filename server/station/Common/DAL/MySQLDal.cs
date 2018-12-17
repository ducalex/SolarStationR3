using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using MySql.Data.MySqlClient;

namespace SolarStationServer.Common.DAL
{
    public class MySQLDal
    {
        public IEnumerable<SolarStationData> getAllSolarStationDatas()
            => query<SolarStationData>("SELECT * FROM `data` ORDER BY datetime");

        /// <summary>
        /// Insert new session.
        /// </summary>
        /// <returns></returns>
        public SolarStationData insertNewSolarStationData(SolarStationData solarStationData)
        {
            int newId = Convert.ToInt32(executeScalar(
                " INSERT INTO `data` " +
                "   (datetime, lightsensorRAW, batteryV, boxtempC, boxhumidityPERC, powermode) " +
                " VALUES(@datetime, @lightsensorRAW, @batteryV, @boxtempC, @boxhumidityPERC, @powermode); " +
                " SELECT LAST_INSERT_ID(); ",
                cmd =>
                {
                    cmd.Parameters.AddWithValue("@datetime", solarStationData.datetime);
                    cmd.Parameters.AddWithValue("@lightsensorRAW", solarStationData.lightsensorRAW);
                    cmd.Parameters.AddWithValue("@batteryV", solarStationData.batteryV);
                    cmd.Parameters.AddWithValue("@boxtempC", solarStationData.boxtempC);
                    cmd.Parameters.AddWithValue("@boxhumidityPERC", solarStationData.boxhumidityPERC);
                    cmd.Parameters.AddWithValue("@powermode", solarStationData.powermode);
                }));

            return query<SolarStationData>(
                "SELECT * FROM `data` WHERE id = @id",
                cmd => cmd.Parameters.AddWithValue("@id", newId))
                .FirstOrDefault();
        }

        /// <summary>
        /// Query.
        /// </summary>
        /// <typeparam name="TItem"></typeparam>
        /// <param name="sql"></param>
        /// <param name="execCmd"></param>
        /// <returns></returns>
        public IEnumerable<TItem> query<TItem>(string sql, Action<MySqlCommand> execCmd = null)
            where TItem : new()
        {
            List<TItem> items = new List<TItem>();

            connectAndExec(
                conn =>
                {
                    MySqlCommand cmd = new MySqlCommand(sql, conn);

                    execCmd?.Invoke(cmd);

                    MySqlDataReader rdr = cmd.ExecuteReader();

                    while (rdr.Read())
                    {
                        TItem item = new TItem();

                        // Check all columns if they exists ...
                        PropertyInfo[] pis = item.GetType().GetProperties(BindingFlags.SetProperty | BindingFlags.Instance | BindingFlags.Static | BindingFlags.Public);

                        for (int index = 0; index < rdr.FieldCount; index++)
                        {
                            string columnName = rdr.GetName(index);
                            Type sqlType = rdr.GetFieldType(index);
                            object value = rdr.GetValue(index);

                            // Get properties
                            PropertyInfo pi = pis.FirstOrDefault(p => p.Name == columnName);
                            if (pi == null)
                                continue;

                            if (pi.PropertyType.GUID == sqlType.GUID)
                            {
                                pi.SetValue(item, value);
                            }
                            else if (Nullable.GetUnderlyingType(pi.PropertyType)?.GUID == sqlType.GUID)
                            {
                                if (value == DBNull.Value)
                                    pi.SetValue(item, null);
                                else
                                    pi.SetValue(item, value);
                            }
                        }

                        items.Add(item);
                    }
                });

            return items;
        }

        /// <summary>
        /// Execute non query
        /// </summary>
        /// <param name="sql"></param>
        /// <param name="execCmd"></param>
        /// <returns></returns>
        public object executeScalar(string sql, Action<MySqlCommand> execCmd = null)
        {
            object value = null;

            connectAndExec(
                conn =>
                {
                    MySqlCommand cmd = new MySqlCommand(sql, conn);

                    execCmd?.Invoke(cmd);

                    value = cmd.ExecuteScalar();
                    if (value == DBNull.Value)
                        value = null;
                });

            return value;
        }

        /// <summary>
        /// Execute in MySQL Connection
        /// </summary>
        /// <param name="exec"></param>
        private void connectAndExec(Action<MySqlConnection> exec)
        {
            MySqlConnection conn = new MySqlConnection("server=192.168.5.214;user=root;database=db_solarstation;port=3306;password=patate");

            try
            {
                conn.Open();

                exec(conn);
            }
            finally
            {
                conn.Close();
            }
        }
    }
}

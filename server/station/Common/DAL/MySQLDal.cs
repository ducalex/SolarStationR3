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
        {
            return query<SolarStationData>("SELECT * FROM `data`");
        }

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

        //public HQCredential insertNewHQCredential(string user, string pass_encrypted)
        //{
        //    int newId = Convert.ToInt32(executeScalar(
        //       " INSERT INTO hqcredential(user, pass_encrypted) " +
        //       "    VALUES(@user, @pass_encrypted); " +
        //       " SELECT LAST_INSERT_ID(); ",
        //       cmd =>
        //       {
        //           cmd.Parameters.AddWithValue("@user", user);
        //           cmd.Parameters.AddWithValue("@pass_encrypted", pass_encrypted);
        //       }));

        //    return getHQCredential(newId);
        //}

        ///// <summary>
        ///// Get existing session.
        ///// </summary>
        ///// <param name="token"></param>
        ///// <returns></returns>
        //public Session getExistingSession(string token)
        //{
        //    return query<Session>(
        //        "SELECT * FROM `session` WHERE authtoken = @token",
        //        cmd => cmd.Parameters.AddWithValue("@token", token))
        //        .FirstOrDefault();
        //}

        ///// <summary>
        ///// Get identity by login.
        ///// </summary>
        ///// <param name="user"></param>
        ///// <returns></returns>
        //public HQCredential getIdentityHQCredential(string user)
        //{
        //    return query<HQCredential>(
        //        "SELECT * FROM `hqcredential` WHERE user = @user",
        //        cmd => cmd.Parameters.AddWithValue("@user", user))
        //        .FirstOrDefault();
        //}

        //public HQCredential getHQCredential(int hqcredentialid)
        //{
        //    return query<HQCredential>(
        //        "SELECT * FROM `hqcredential` WHERE id = @hqcredentialid",
        //        cmd => cmd.Parameters.AddWithValue("@hqcredentialid", hqcredentialid))
        //        .FirstOrDefault();
        //}

        //public HQData insertNewHQData(int hqcredentialid, DateTime date, string json)
        //{
        //    int newId = Convert.ToInt32(executeScalar(
        //       " INSERT INTO hqdata(hqcredentialid, date, json) " +
        //       "    VALUES(@hqcredentialid, @date, @json); " +
        //       " SELECT LAST_INSERT_ID(); ",
        //       cmd =>
        //       {
        //           cmd.Parameters.AddWithValue("@hqcredentialid", hqcredentialid);
        //           cmd.Parameters.AddWithValue("@date", date);
        //           cmd.Parameters.AddWithValue("@json", json);
        //       }));

        //    return query<HQData>(
        //        "SELECT * FROM `hqdata` WHERE id = @hqdataid",
        //        cmd => cmd.Parameters.AddWithValue("@hqdataid", newId))
        //        .FirstOrDefault();
        //}

        //public IEnumerable<HQData> getAllHQDatas(int hqcredentialid)
        //{
        //    return query<HQData>(
        //        "SELECT * FROM `hqdata` WHERE hqcredentialid = @hqcredentialid",
        //        cmd => cmd.Parameters.AddWithValue("@hqcredentialid", hqcredentialid));
        //}

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

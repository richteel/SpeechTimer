// Structure returned by https://worldtimeapi.org/api/ip and https://worldtimeapi.org/api/timezone/America/New_York


struct InternetTimeStruct {
  const char* abbreviation;           // "EDT"
  const char* client_ip;              // "173.79.173.100"
  const char* datetime;               // "2023-09-17T17:54:56.042498-04:00"
  int day_of_week;                    // 0
  int day_of_year;                    // 260
  bool dst;                           // true
  const char* dst_from;               // "2023-03-12T07:00:00+00:00"
  int dst_offset;                     // 3600
  const char* dst_until;              // "2023-11-05T06:00:00+00:00"
  int raw_offset;                     // -18000
  const char* timezone;               // "America/New_York"
  long unixtime;                      // 1694987696
  const char* utc_datetime;           // "2023-09-17T21:54:56.042498+00:00"
  const char* utc_offset = "+00:00";  // "-04:00"
  int week_number;                    // 37
};
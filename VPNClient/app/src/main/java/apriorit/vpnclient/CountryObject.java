package apriorit.vpnclient;

public class CountryObject {
    private int    flagId;
    private String countryName;
    private String ipAddr;
    private String port;

    public CountryObject() {
        this.flagId = 0;
        this.countryName = "localhost";
        this.ipAddr = "127.0.0.1";
        this.port = "8000";
    }

    public CountryObject(int flagId, String countryName, String ipAddr, String port) {
        this.flagId = flagId;
        this.countryName = countryName;
        this.ipAddr = ipAddr;
        this.port = port;
    }

    public int getFlagId()         { return flagId;      }
    public String getCountryName() { return countryName; }
    public String getIpAddr()      { return ipAddr;      }
    public String getPort()        { return port;        }

    public void setFlagId(int num)         {  this.flagId = num;      }
    public void setCountryName(String str) {  this.countryName = str; }
    public void setIpAddr(String str)      {  this.ipAddr = str;      }
    public void setPort(String str)        {  this.port = str;        }
}
package apriorit.vpnclient;

public class CountryObject {
    private int    flagId;
    private String countryName;
    private String ipAddr;
    private String port;

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
}
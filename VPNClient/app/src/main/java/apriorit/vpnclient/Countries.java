package apriorit.vpnclient;

public class Countries {
    private CountryObject[] countries;

    public Countries(CountryObject[] countries) {
        this.countries = countries;
    }

    public Integer[] getCountriesIds() {
        Integer[] result = new Integer[countries.length];
        for(int i = 0; i < countries.length; ++i) {
            result[i] = countries[i].getFlagId();
        }
        return result;
    }

    public String[] getCountriesNames() {
        String[] result = new String[countries.length];
        for(int i = 0; i < countries.length; ++i) {
            result[i] = countries[i].getCountryName();
        }
        return result;
    }

    public String[] getIpAddresses() {
        String[] result = new String[countries.length];
        for(int i = 0; i < countries.length; ++i) {
            result[i] = countries[i].getIpAddr();
        }
        return result;
    }

    public String[] getServerPorts() {
        String[] result = new String[countries.length];
        for(int i = 0; i < countries.length; ++i) {
            result[i] = countries[i].getPort();
        }
        return result;
    }
}
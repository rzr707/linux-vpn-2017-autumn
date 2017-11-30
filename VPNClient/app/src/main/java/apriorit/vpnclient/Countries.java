package apriorit.vpnclient;




public class Countries {
    private CountryObject[] countries;
    private int count_at_init = 0;


    public Countries(CountryObject[] countries) {
        this.countries = countries;
        count_at_init = countries.length;
    }

    public int GetCount(){
        return countries.length;
    }

    public void AddNewCountry(String countryName, String ipAddr, String port) {
        if(count_at_init==countries.length) {
            CountryObject[] new_counties = new CountryObject[countries.length + 1];
            for (int i = 0; i < countries.length; i++)
                new_counties[i] = countries[i];

            new_counties[countries.length] = new CountryObject(-1, countryName, ipAddr, port);
            countries = new_counties;
        }
        else {
            countries[countries.length-1].setCountryName(countryName);
            countries[countries.length-1].setIpAddr(ipAddr);
            countries[countries.length-1].setPort(port);
        }
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
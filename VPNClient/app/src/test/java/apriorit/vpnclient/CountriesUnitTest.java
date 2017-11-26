package apriorit.vpnclient;

import org.junit.Test;

import static org.junit.Assert.*;

public class CountriesUnitTest {
    final Countries countries = new Countries(new CountryObject[] {
            new CountryObject(R.drawable.ic_flag_of_france, "France",
                    "5.135.153.169", "8000"),
            new CountryObject(R.drawable.ic_flag_of_the_united_states, "USA",
                    "192.241.141.236", "8000")
    });

    private CountryObject fakeCountry = new CountryObject(R.drawable.ic_flag_of_the_united_states,
            "Zimbabwe", "10.20.30.40", "1234");

    @Test
    public void countriesGetCountriesIds() {
        Integer[] countriesIds = countries.getCountriesIds();

        assertEquals(countriesIds.length, countries.getCountriesIds().length);
        assertEquals(countriesIds[0], (Integer)R.drawable.ic_flag_of_france);
        assertEquals(countriesIds[1], (Integer)R.drawable.ic_flag_of_the_united_states);
        assertNotEquals(countriesIds[0], (Integer)fakeCountry.getFlagId());
    }

    @Test
    public void countriesGetCountriesName() {
        String[] countriesNames = countries.getCountriesNames();

        assertEquals(countriesNames[0], "France");
        assertEquals(countriesNames[1], "USA");
        assertNotEquals(countriesNames[0], fakeCountry.getCountryName());
    }

    @Test
    public void countriesGetCountriesIpAddresses() {
        String[] countriesIpAddresses = countries.getIpAddresses();

        assertEquals(countriesIpAddresses[0], "5.135.153.169");
        assertEquals(countriesIpAddresses[1], "192.241.141.236");
        assertNotEquals(countriesIpAddresses[1], "10.20.30.40");
    }

    @Test
    public void countriesGetServerPorts() {
        String[] countriesPorts = countries.getServerPorts();

        assertEquals(countriesPorts[0], "8000");
        assertEquals(countriesPorts[1], "8000");
        assertEquals(countriesPorts[0], countriesPorts[1]);
        assertNotEquals(countriesPorts[0], "12345");
    }
}

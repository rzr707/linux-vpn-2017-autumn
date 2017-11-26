package apriorit.vpnclient;

import org.junit.Test;
import static org.junit.Assert.*;

public class CountryObjectUnitTest {
    private CountryObject countryObject
            = new CountryObject(R.drawable.ic_flag_of_ukraine,
            "Ukraine",
            "12.11.136.75",
            "7777");

    @Test
    public void countryObjectGetFlagId() {
        assertEquals(countryObject.getFlagId(), R.drawable.ic_flag_of_ukraine);
        assertNotEquals(countryObject.getFlagId(), R.drawable.ic_flag_of_the_united_states);
    }

    @Test
    public void countryObjectGetCountryName() {
        assertEquals(countryObject.getCountryName(), "Ukraine");
        assertNotEquals(countryObject.getCountryName(), "Rwanda");
    }

    @Test
    public void countryObjectGetIpAddress() {
        assertEquals("12.11.136.75", countryObject.getIpAddr());
        assertNotEquals("20.30.40.50", countryObject.getIpAddr());
    }

    @Test
    public void countryObjectGetPort() {
        assertEquals("7777", countryObject.getPort());
        assertNotEquals("8000", countryObject.getPort());
    }
}

package apriorit.vpnclient;

/**
 * Created by admin on 01.11.2017.
 */

import android.util.Log;

import com.wolfssl.*;

class MyLoggingCallback implements WolfSSLLoggingCallback
{
    public void loggingCallback(int logLevel, String logMessage) {
        System.out.printf("lvl:%d msg:%s%n", logLevel, logMessage);
        Log.i("LOG_CALLBACK", "lvl: " + logLevel + ", msg: " + logMessage);
    }
}
package apriorit.vpnclient;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

import static android.content.Context.MODE_PRIVATE;
import static apriorit.vpnclient.VpnClient.EXTRA_MESSAGE;
import static apriorit.vpnclient.VpnClient.Prefs.BOOTS;

public class ReceiveStartUp extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED)) {
            SharedPreferences prefs = context.getSharedPreferences(VpnClient.Prefs.NAME, MODE_PRIVATE);
            if (prefs.getString(BOOTS, "").equals("on")) {
                Intent i = new Intent(context, VpnClient.class);
                i.putExtra(EXTRA_MESSAGE, "START_BOOT");
                i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(i);
            }
        }
    }
}

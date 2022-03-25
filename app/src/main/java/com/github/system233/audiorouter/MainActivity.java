package com.github.system233.audiorouter;

import androidx.appcompat.app.AppCompatActivity;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;
import android.view.View;

import com.github.system233.audiorouter.databinding.ActivityMainBinding;
public class MainActivity extends AppCompatActivity{
    static final String TAG="MainActivity";
    static final String CONFIG_HOST="ServerName";
    static final String CONFIG_PORT="ServerPort";
    ActivityMainBinding binding;
    boolean isRunning=false;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding=ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        SharedPreferences preferences=getSharedPreferences("config",MODE_PRIVATE);
        binding.editServerName.setText(preferences.getString(CONFIG_HOST,"127.0.0.1"));
        binding.editServerPort.setText(preferences.getInt(CONFIG_PORT,10000)+"");
        binding.btnConnect.setOnClickListener((View view)->{
            try{
                if(!isRunning) {
                    String ip = binding.editServerName.getText().toString();
                    int port = Integer.parseInt(binding.editServerPort.getText().toString());
                    start(ip, port);
                    SharedPreferences.Editor editor = preferences.edit();
                    editor.putString(CONFIG_HOST, ip);
                    editor.putInt(CONFIG_PORT, port);
                    editor.apply();
                }else{
                    stop();
                }
            }catch (Throwable throwable){
                Log.e(TAG, "onCreate: ", throwable);
            }
        });
    }
    void updateConnectState(){
        if(!isRunning){
            binding.btnConnect.setText(R.string.text_connect);
        }else{
            binding.btnConnect.setText(R.string.text_disconnect);
        }
        binding.editServerName.setEnabled(!isRunning);
        binding.editServerPort.setEnabled(!isRunning);
    }
    void start(String server,int port){
        stop();
        Intent intent=new Intent(this,AudioService.class);
        intent.putExtra("ServerName",server);
        intent.putExtra("ServerPort",port);
        isRunning=startForegroundService(intent)!=null;
        updateConnectState();
    }
    void stop(){
        Intent intent=new Intent(this,AudioService.class);
        isRunning=isRunning&&!stopService(intent);
        updateConnectState();
//        isRunning=true;
    }
}
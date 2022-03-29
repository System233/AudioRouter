package com.github.system233.audiorouter;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.IBinder;
import android.util.Log;

import androidx.annotation.Nullable;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class AudioService extends Service implements Runnable, Closeable {
    static final String TAG="AudioService";
    AudioTrack mAudioTrack;
    String mServerIP;
    int mServerPort,mNotId;
    long instance;
    Thread mThread;
    byte[] mBuffer=new byte[0x10000];
    float[]mPCMBuffer=new float[mBuffer.length/4];
    static {
        System.loadLibrary("kcp_c");
    }
    @Override
    public void run() {
        mThread=Thread.currentThread();
        start(mServerIP,mServerPort);
    }
    public void handle(byte[]buf,int len){
        try{
//        Log.d(TAG, "handle: "+buf.length);
            if(Thread.interrupted()&&instance!=0){
                stop();
                instance=0;
                return;
            }
            if(Format.check(buf)){
                Log.d(TAG, "do setup");
                mAudioTrack=setup(Format.from(buf));
                mAudioTrack.play();
                Log.d(TAG, "setup: "+mAudioTrack);
            }else if(mAudioTrack!=null){
                ByteBuffer buffer=ByteBuffer.wrap(buf,0,len);
                buffer.order(ByteOrder.LITTLE_ENDIAN);
                for(int i=0;i<len/4;++i){
                    mPCMBuffer[i]=buffer.getFloat();
                }
                mAudioTrack.write(mPCMBuffer,0,len/4, AudioTrack.WRITE_NON_BLOCKING);
            }
        }catch (Throwable throwable){
            Log.e(TAG, "handle: ",throwable );
        }
    }
    native void start(String host,int port);
    native void stop();
    public boolean alive(){
        return mThread!=null&&mAudioTrack!=null;
    }
    public void start(){
        close();
        mThread=new Thread(this);
        mThread.start();
    }
    @Override
    public void close() {
        if(mThread!=null){
            mThread.interrupt();
            mThread=null;
        }
    }
    long mPing=0;
    NotificationManager notificationManager;
    public void ping(long ping){
        if(ping!=mPing){
            mPing=ping;
            if(notificationManager==null){
                notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            }
            notificationManager.notify(mNotId,make(String.format("传输延迟 %dms",ping)));
        }
    }
    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind: ");
        mServerIP=intent.getStringExtra("ServerName");
        mServerPort=intent.getIntExtra("ServerPort",10000);
        start();
        return null;
    }
    Notification make(String text){
        return new Notification.Builder(this,getClass().toString())
                .setSmallIcon(R.mipmap.ic_launcher)
                .setContentTitle(text)
                .setContentText(getString(R.string.service_message))
                .build();
    }
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand: ");
        mServerIP=intent.getStringExtra("ServerName");
        mServerPort=intent.getIntExtra("ServerPort",10000);
        mNotId=startId;
        NotificationChannel notificationChannel = new NotificationChannel(getClass().toString(),
                getString(R.string.app_name), NotificationManager.IMPORTANCE_HIGH);
        notificationChannel.enableLights(true);
        notificationChannel.setLightColor(Color.RED);
        notificationChannel.setShowBadge(true);
        notificationChannel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC);
        NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        notificationManager.createNotificationChannel(notificationChannel);
        start();
        startForeground(mNotId, make(getString(R.string.service_title)));
        return super.onStartCommand(intent, flags, mNotId);
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy: ");
        super.onDestroy();
        stop();
        stopForeground(true);
        NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        notificationManager.deleteNotificationChannel(getClass().toString());
    }

    public static class Format{
        public int id;
        public int sampleRate;
        public int bitsPerSample;
        public int channels;
        public int format;
        static public int SIZE=16;
        static public Format from(byte[]buf){
            ByteBuffer byteBuffer=ByteBuffer.wrap(buf);
            byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
            Format format=new Format();
            format.id=byteBuffer.getInt();
            format.sampleRate=byteBuffer.getInt();
            format.bitsPerSample=byteBuffer.getInt();
            format.channels=byteBuffer.getInt();
            format.format=byteBuffer.getInt();
            return format;
        }
        static public boolean check(byte[]buf){
//            ByteBuffer byteBuffer=ByteBuffer.wrap(buf);
//            byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
//            Log.d(TAG, "check: "+byteBuffer.getInt()+":"+String.format("%x,%x,%x,%x",buf[0],buf[1],buf[2],buf[3])+":"+(buf.length>4&&buf[0]==0x78&&buf[1]==0x56&&buf[2]==0x34&&buf[3]==0x12));
            return buf.length>4&&buf[0]==0x78&&buf[1]==0x56&&buf[2]==0x34&&buf[3]==0x12;
        }
    }
//    public AudioService(){}
//    public AudioService(String serverIP, int serverPort){
//        mServerIP=serverIP;
//        mServerPort=serverPort;
//    }
//    static public Thread build(String serverIP,int serverPort){
//        return new Thread(new AudioService(serverIP,serverPort));
//    }
    AudioTrack setup(Format format){
        Log.d(TAG, "setup: "+format.toString());
//        return  new AudioTrack(AudioManager.STREAM_MUSIC, format.sampleRate,
//                AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_FLOAT,
//                AudioTrack.getMinBufferSize(format.sampleRate,format.channels,AudioFormat.ENCODING_PCM_FLOAT), AudioTrack.MODE_STREAM);
        return new AudioTrack.Builder()
                .setAudioFormat(new AudioFormat.Builder()
                        .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                        .setSampleRate(format.sampleRate)
                        .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
                        .build())
                .setBufferSizeInBytes(AudioTrack.getMinBufferSize(format.sampleRate,format.channels,AudioFormat.ENCODING_PCM_FLOAT)<<2)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .build())
                .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                .build();
    }
}

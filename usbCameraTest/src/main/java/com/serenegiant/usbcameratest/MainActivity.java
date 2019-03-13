/*
 *  UVCCamera
 *  library and sample to access to UVC web camera on non-rooted Android device
 *
 * Copyright (c) 2014-2017 saki t_saki@serenegiant.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *  All files in the folder are under this Apache License, Version 2.0.
 *  Files in the libjpeg-turbo, libusb, libuvc, rapidjson folder
 *  may have a different license, see the respective files.
 */

package com.serenegiant.usbcameratest;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.SurfaceTexture;
import android.hardware.usb.UsbDevice;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Surface;
import android.widget.TextView;
import android.widget.Toast;

import com.huawei.hiaidemo.Constant;
import com.huawei.hiaidemo.ModelManager;
import com.serenegiant.common.BaseActivity;
import com.serenegiant.usb.CameraDialog;
import com.serenegiant.usb.DeviceFilter;
import com.serenegiant.usb.IButtonCallback;
import com.serenegiant.usb.IFrameCallback;
import com.serenegiant.usb.IStatusCallback;
import com.serenegiant.usb.USBMonitor;
import com.serenegiant.usb.USBMonitor.OnDeviceConnectListener;
import com.serenegiant.usb.USBMonitor.UsbControlBlock;
import com.serenegiant.usb.UVCCamera;
import com.serenegiant.widget.SimpleUVCCameraTextureView;
import com.tencent.squeezencnn.SqueezeNcnn;

import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import static android.graphics.Color.blue;
import static android.graphics.Color.green;
import static android.graphics.Color.red;
import static com.huawei.hiaidemo.Constant.AI_OK;
import static com.huawei.hiaidemo.Constant.meanValueOfBlue;
import static com.huawei.hiaidemo.Constant.meanValueOfGreen;
import static com.huawei.hiaidemo.Constant.meanValueOfRed;

public final class MainActivity extends BaseActivity implements CameraDialog.CameraDialogParent {

	private static final String TAG = "UVCCameraAI";

	private final Object mSync = new Object();
    // for accessing USB and USB camera
    private USBMonitor mUSBMonitor;
	private UVCCamera mUVCCamera;
	private SimpleUVCCameraTextureView mUVCCameraView;
	// for open&start / stop&close camera preview
	private Surface mPreviewSurface;
	private Toast mToast;

	/** add for UVCCameraAI begin **/
	private boolean useNPU;
	private AssetManager mgr;
	private TextView mClassifyInfo;
	private TextView mClassifySubInfo;
	private TextView mClassifyTime;
	// single thread executor for hiai
	ExecutorService mHIAIExecutorService;

	private SqueezeNcnn squeezencnn = new SqueezeNcnn();
	/** add for UVCCameraAI end **/

	private static final boolean USE_HIAI = false;
	private int mBitmapWidth;
	private int mBitmapHeight;

	@Override
	protected void onCreate(final Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		setContentView(R.layout.activity_main);

		/** add for UVCCameraAI begin **/
		if (USE_HIAI) {
			initHIAI();
			mBitmapWidth = Constant.RESIZED_WIDTH;
			mBitmapHeight = Constant.RESIZED_HEIGHT;
		} else {
			initSqueezeNCNN();
			mBitmapWidth = Constant.RESIZED_WIDTH_NCNN;
			mBitmapHeight = Constant.RESIZED_HEIGHT_NCNN;
		}

		mHIAIExecutorService = Executors.newSingleThreadExecutor();

		mClassifyInfo = (TextView) findViewById(R.id.classify_info);
		mClassifySubInfo = (TextView) findViewById(R.id.classify_sub_info);
		mClassifyTime = (TextView) findViewById(R.id.classify_time);
		/** add for UVCCameraAI end **/

		mUVCCameraView = (SimpleUVCCameraTextureView)findViewById(R.id.UVCCameraTextureView1);
		mUVCCameraView.setAspectRatio(UVCCamera.DEFAULT_PREVIEW_WIDTH / (float) UVCCamera.DEFAULT_PREVIEW_HEIGHT);

		mUSBMonitor = new USBMonitor(this, mOnDeviceConnectListener);
	}

	@Override
	protected void onStart() {
		super.onStart();
		mUSBMonitor.register();

		new Handler(getMainLooper()).postDelayed(new Runnable() {
			@Override
			public void run() {
				final List<DeviceFilter> filter = DeviceFilter.getDeviceFilters(
						MainActivity.this, com.serenegiant.uvccamera.R.xml.device_filter);
				List<UsbDevice> usbDevices = mUSBMonitor.getDeviceList(filter.get(0));
				if (usbDevices == null || usbDevices.size() == 0) {
					Toast.makeText(MainActivity.this, "no usb camera connect", Toast.LENGTH_SHORT).show();
					finish();
				} else {
					//Toast.makeText(MainActivity.this, "usb camera connected", Toast.LENGTH_SHORT).show();
					boolean result = mUSBMonitor.requestPermission(usbDevices.get(0));
				}
			}
		}, 100);


		synchronized (mSync) {
			if (mUVCCamera != null) {
				mUVCCamera.startPreview();
			}
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
	}

	@Override
	protected void onStop() {
		synchronized (mSync) {
			if (mUVCCamera != null) {
				mUVCCamera.stopPreview();
			}
			if (mUSBMonitor != null) {
				mUSBMonitor.unregister();
			}
		}
		super.onStop();
	}

	@Override
	protected void onDestroy() {
		synchronized (mSync) {
			releaseCamera();
			if (mToast != null) {
				mToast.cancel();
				mToast = null;
			}
			if (mUSBMonitor != null) {
				mUSBMonitor.destroy();
				mUSBMonitor = null;
			}
		}
		mUVCCameraView = null;
		super.onDestroy();

        /** add for UVCCameraAI begin **/
		if (USE_HIAI) {
			int result = ModelManager.unloadModelSync();

			if (AI_OK == result) {
				Log.d(TAG, "unload model success.");
			} else {
				Log.d(TAG, "unload model fail.");
			}
		}
        /** add for UVCCameraAI end **/
	}

	private final OnDeviceConnectListener mOnDeviceConnectListener = new OnDeviceConnectListener() {
		@Override
		public void onAttach(final UsbDevice device) {
			//Toast.makeText(MainActivity.this, "USB_DEVICE_ATTACHED", Toast.LENGTH_SHORT).show();
			Log.d(TAG, "OnDeviceConnectListener -- >USB_DEVICE_ATTACHED");
		}

		@Override
		public void onConnect(final UsbDevice device, final UsbControlBlock ctrlBlock, final boolean createNew) {
			releaseCamera();
			queueEvent(new Runnable() {
				@Override
				public void run() {
					final UVCCamera camera = new UVCCamera();
					camera.open(ctrlBlock);
					camera.setStatusCallback(new IStatusCallback() {
						@Override
						public void onStatus(final int statusClass, final int event, final int selector,
											 final int statusAttribute, final ByteBuffer data) {
							runOnUiThread(new Runnable() {
								@Override
								public void run() {
									final Toast toast = Toast.makeText(MainActivity.this, "onStatus(statusClass=" + statusClass
											+ "; " +
											"event=" + event + "; " +
											"selector=" + selector + "; " +
											"statusAttribute=" + statusAttribute + "; " +
											"data=...)", Toast.LENGTH_SHORT);
									synchronized (mSync) {
										if (mToast != null) {
											mToast.cancel();
										}
										toast.show();
										mToast = toast;
									}
								}
							});
						}
					});
					camera.setButtonCallback(new IButtonCallback() {
						@Override
						public void onButton(final int button, final int state) {
							runOnUiThread(new Runnable() {
								@Override
								public void run() {
									final Toast toast = Toast.makeText(MainActivity.this, "onButton(button=" + button + "; " +
											"state=" + state + ")", Toast.LENGTH_SHORT);
									synchronized (mSync) {
										if (mToast != null) {
											mToast.cancel();
										}
										mToast = toast;
										toast.show();
									}
								}
							});
						}
					});
//					camera.setPreviewTexture(camera.getSurfaceTexture());
					if (mPreviewSurface != null) {
						mPreviewSurface.release();
						mPreviewSurface = null;
					}
					try {
						camera.setPreviewSize(UVCCamera.DEFAULT_PREVIEW_WIDTH, UVCCamera.DEFAULT_PREVIEW_HEIGHT, UVCCamera.FRAME_FORMAT_MJPEG);
					} catch (final IllegalArgumentException e) {
						// fallback to YUV mode
						try {
							camera.setPreviewSize(UVCCamera.DEFAULT_PREVIEW_WIDTH, UVCCamera.DEFAULT_PREVIEW_HEIGHT, UVCCamera.DEFAULT_PREVIEW_MODE);
						} catch (final IllegalArgumentException e1) {
							camera.destroy();
							return;
						}
					}
					final SurfaceTexture st = mUVCCameraView.getSurfaceTexture();
					if (st != null) {
						mPreviewSurface = new Surface(st);
						camera.setPreviewDisplay(mPreviewSurface);
						camera.setFrameCallback(mIFrameCallback, UVCCamera.PIXEL_FORMAT_RGB565/*UVCCamera.PIXEL_FORMAT_NV21*/);
						camera.startPreview();
					}
					synchronized (mSync) {
						mUVCCamera = camera;
					}
				}
			}, 0);
		}

		@Override
		public void onDisconnect(final UsbDevice device, final UsbControlBlock ctrlBlock) {
			// XXX you should check whether the coming device equal to camera device that currently using
			releaseCamera();
		}

		@Override
		public void onDettach(final UsbDevice device) {
			Toast.makeText(MainActivity.this, "USB_DEVICE_DETACHED", Toast.LENGTH_SHORT).show();
		}

		@Override
		public void onCancel(final UsbDevice device) {
		}
	};

	private synchronized void releaseCamera() {
		synchronized (mSync) {
			if (mUVCCamera != null) {
				try {
					mUVCCamera.setStatusCallback(null);
					mUVCCamera.setButtonCallback(null);
					mUVCCamera.close();
					mUVCCamera.destroy();
				} catch (final Exception e) {
					//
				}
				mUVCCamera = null;
			}
			if (mPreviewSurface != null) {
				mPreviewSurface.release();
				mPreviewSurface = null;
			}
		}
	}

	/**
	 * to access from CameraDialog
	 * @return
	 */
	@Override
	public USBMonitor getUSBMonitor() {
		return mUSBMonitor;
	}

	@Override
	public void onDialogResult(boolean canceled) {
		if (canceled) {
			runOnUiThread(new Runnable() {
				@Override
				public void run() {
					// FIXME
				}
			}, 0);
		}
	}

	// if you need frame data as byte array on Java side, you can use this callback method with UVCCamera#setFrameCallback
	// if you need to create Bitmap in IFrameCallback, please refer following snippet.
	private final Bitmap bitmap = Bitmap.createBitmap(UVCCamera.DEFAULT_PREVIEW_WIDTH, UVCCamera.DEFAULT_PREVIEW_HEIGHT, Bitmap.Config.RGB_565);
	private float[] mPixels;
	private long mLastMillis = -1;
	private static final long HIAI_PREDICT_INTERVAL = 1000;
	private final IFrameCallback mIFrameCallback = new IFrameCallback() {
		@Override
		public void onFrame(final ByteBuffer frame) {
			frame.clear();
			if (System.currentTimeMillis() - mLastMillis >= HIAI_PREDICT_INTERVAL) {
				mLastMillis = System.currentTimeMillis();
				synchronized (bitmap) {
					if (USE_HIAI) {
						bitmap.copyPixelsFromBuffer(frame);
						Bitmap classifyImage = Bitmap.createScaledBitmap(bitmap, mBitmapWidth, mBitmapHeight, false);
						mPixels = getPixel(classifyImage, mBitmapWidth, mBitmapHeight);
						recycleBitmap(classifyImage);
						mHIAIExecutorService.execute(new Runnable() {
							@Override
							public void run() {
								final String[] predictedClass = ModelManager.runModelSync("InceptionV3", mPixels);
								runOnUiThread(new Runnable() {
									@Override
									public void run() {
										mClassifyInfo.setText(predictedClass[0]);
										mClassifySubInfo.setText(predictedClass[1]);
										mClassifyTime .setText(predictedClass[2]);
									}
								});
							}
						});
					} else {
						bitmap.copyPixelsFromBuffer(frame);
						Bitmap rgba = bitmap.copy(Bitmap.Config.ARGB_8888, true);

						// resize to 227x227
						final Bitmap classifyImage = Bitmap.createScaledBitmap(rgba, mBitmapWidth, mBitmapHeight, false);
						mHIAIExecutorService.execute(new Runnable() {
							@Override
							public void run() {
								final String result = squeezencnn.Detect(classifyImage);
								recycleBitmap(classifyImage);
								runOnUiThread(new Runnable() {
									@Override
									public void run() {
										mClassifyInfo.setText(result);
										mClassifySubInfo.setText("");
										mClassifyTime .setText("");
									}
								});
							}
						});
					}
				}
			}
			//mImageView.post(mUpdateImageTask);
		}
	};

	private void recycleBitmap(Bitmap classifyImage) {
		if (classifyImage != null && !classifyImage.isRecycled()) {
			classifyImage.recycle();
			classifyImage = null;
		}
		System.gc();
	}

	/** add for UVCCameraAI begin **/
	private float[] getPixel(Bitmap bitmap, int resizedWidth, int resizedHeight) {
		int channel = 3;
		float[] buff = new float[channel * resizedWidth * resizedHeight];

		int rIndex, gIndex, bIndex;
		for (int i = 0; i < resizedHeight; i++) {
			for (int j = 0; j < resizedWidth; j++) {
				bIndex = i * resizedWidth + j;
				gIndex = bIndex + resizedWidth * resizedHeight;
				rIndex = gIndex + resizedWidth * resizedHeight;

				int color = bitmap.getPixel(j, i);

				buff[bIndex] = (float) ((blue(color) - meanValueOfBlue))/255;
				buff[gIndex] = (float) ((green(color) - meanValueOfGreen))/255;
				buff[rIndex] = (float) ((red(color) - meanValueOfRed))/255;
			}
		}

		return buff;
	}

	private void initHIAI() {
		String platformversion = /*getProperty("ro.config.hiaiversion", "kirin960")*/"kirin960";
		Log.i(TAG, "platformversion : " + platformversion);

		if (platformversion.equals("") || platformversion.equals("000.000.000.000")) {
			useNPU = false;
		} else {
			useNPU = true;
			/** load libhiai.so */
			boolean isSoLoadSuccess = ModelManager.init();
			if (isSoLoadSuccess) {
				Log.d(TAG, "load libhiai.so success");
			} else {
				Log.d(TAG, "load libhiai.so fail.");
			}
			/*init classify labels */
			initLabels();
		}

		mgr = getResources().getAssets();
		new loadModelTask().execute();
	}

	private class loadModelTask extends AsyncTask<Void, Void, Integer> {
		@Override
		protected Integer doInBackground(Void... voids) {

			int ret = ModelManager.loadModelSync("InceptionV3", mgr);

			return ret;
		}

		@Override
		protected void onPostExecute(Integer result) {
			super.onPostExecute(result);

			if (AI_OK == result) {
				Log.d(TAG, "load model success.");
			} else {
				Log.d(TAG, "load model fail.");
			}
		}
	}

	private void initLabels() {
		byte[] labels;
		try {
			InputStream assetsInputStream = getAssets().open("labels.txt");
			int available = assetsInputStream.available();
			labels = new byte[available];
			assetsInputStream.read(labels);
			assetsInputStream.close();
			ModelManager.initLabels(labels);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private String getProperty(String key, String defaultvalue) {
		String value = defaultvalue;
		try {
			Class<?> c = Class.forName("android.os.SystemProperties");
			Method get = c.getMethod("get", String.class);
			value = (String) (get.invoke(c, key));
			Log.i(TAG, "original verison is : " + value);

			if (value == null) {
				value = "";
			}
		} catch (Exception e) {
			Log.e(TAG, "error info : " + e.getMessage());
			value = "";
		} finally {
			return value;
		}
	}

	private void initSqueezeNCNN() {

		try {
			byte[] param = null;
			byte[] bin = null;
			byte[] words = null;

			{
				InputStream assetsInputStream = getAssets().open("squeezenet_v1.1.param.bin");
				int available = assetsInputStream.available();
				param = new byte[available];
				int byteCode = assetsInputStream.read(param);
				assetsInputStream.close();
			}
			{
				InputStream assetsInputStream = getAssets().open("squeezenet_v1.1.bin");
				int available = assetsInputStream.available();
				bin = new byte[available];
				int byteCode = assetsInputStream.read(bin);
				assetsInputStream.close();
			}
			{
				InputStream assetsInputStream = getAssets().open("synset_words.txt");
				int available = assetsInputStream.available();
				words = new byte[available];
				int byteCode = assetsInputStream.read(words);
				assetsInputStream.close();
			}

			squeezencnn.Init(param, bin, words);
		} catch (IOException e) {
			Log.e(TAG, "initSqueezeNCNN error");
		}
	}
	/** add for UVCCameraAI end **/
}

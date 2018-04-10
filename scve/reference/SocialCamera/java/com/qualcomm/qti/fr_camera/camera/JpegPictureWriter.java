/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

package com.qualcomm.qti.fr_camera.camera;

import android.content.Context;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Environment;
import android.os.Handler;
import android.provider.MediaStore;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Objects;
import java.util.TimeZone;

/**
 * This class asynchronously writes an image to disk, and then requests that MediaStore index it
 */
public abstract class JpegPictureWriter {
    private static final String TAG = JpegPictureWriter.class.getSimpleName();
    private static final String FILENAME_PATTERN = "IMG_%s_%d.jpg";
    private static final String TIMESTAMP_PATTERN = "yyyyMMdd_HHmmss";

    private final Context context;
    private final Handler handler;
    private final Date timestamp;
    private final ByteBuffer jpegData;

    /**
     * Construct a new {@link JpegPictureWriter}
     * @param context the active {@link Context}
     * @param jpegData a {@link ByteBuffer} containing JPEG data
     * @throws NullPointerException if context or jpegData are null
     */
    public JpegPictureWriter(Context context, ByteBuffer jpegData) {
        Objects.requireNonNull(context, "context may not be null");
        Objects.requireNonNull(jpegData, "jpegData may not be null");

        this.context = context;

        handler = new Handler();
        timestamp = new Date();
        this.jpegData = jpegData;

        writerTask.execute();
    }

    /**
     * Subclasses must override this method to handle any required actions after the image has been
     * successfully written to disk. This method will be invoked on the UI thread.
     * @param path the path to the image {@link File}
     * @param uri the {@link Uri} of the image in the {@link MediaStore}
     */
    public abstract void onWriteComplete(File path, Uri uri);

    /**
     * Subclasses must override this method to handle any required actions if writing or indexing
     * the image fails. This method will be invoked on the UI thread.
     */
    public abstract void onWriteFailed();

    @SuppressWarnings("FieldCanBeLocal")
    private final AsyncTask<Void, Void, Boolean> writerTask = new AsyncTask<Void, Void, Boolean>() {
        @Override
        protected Boolean doInBackground(Void... params) {
            File pic = writeToFile();
            if (pic == null) {
                return false;
            }

            Log.v(TAG, "Successfully wrote picture to '" + pic + "', requesting indexing");
            indexImage(pic);

            return true;
        }

        @Override
        protected void onPostExecute(Boolean result) {
            if (!result) {
                onWriteFailed();
            }
        }

        private File writeToFile() {
            SimpleDateFormat sdf = new SimpleDateFormat(TIMESTAMP_PATTERN, Locale.US);
            sdf.setTimeZone(TimeZone.getTimeZone("UTC"));

            File path = Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_PICTURES);

            if (!path.mkdirs() && !path.isDirectory()) {
                Log.e(TAG, "Failed creating path '" + path + "' to save picture");
                return null;
            }

            File pic = null;
            for (int i = 0; i < 100; i++) {
                File f = new File(path, String.format(Locale.US, FILENAME_PATTERN,
                        sdf.format(timestamp), i));
                try {
                    if (f.createNewFile()) {
                        pic = f;
                        break;
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Error creating file '" + f + "' for saving picture", e);
                    return null;
                }
            }

            if (pic == null) {
                Log.e(TAG, "Unable to find an unused filename to save picture");
                return null;
            }

            try (
                    FileOutputStream fos = new FileOutputStream(pic);
                    FileChannel channel = fos.getChannel()
            ) {
                jpegData.position(0);
                channel.write(jpegData);
            } catch (IOException e) {
                Log.e(TAG, "Failed writing file '" + pic + "' for saving picture", e);
                //noinspection ResultOfMethodCallIgnored
                pic.delete(); // Remove the failed picture file
                return null;
            }

            return pic;
        }

        private void indexImage(File jpegPath) {
            MediaScannerConnection.scanFile(context, new String[] { jpegPath.getPath() },
                    new String[] { "image/jpeg" }, scanCompletedListener);
        }
    };

    private final MediaScannerConnection.OnScanCompletedListener scanCompletedListener =
            new MediaScannerConnection.MediaScannerConnectionClient() {
        @Override
        public void onMediaScannerConnected() {
        }

        @Override
        public void onScanCompleted(final String path, final Uri uri) {
            // N.B. This is invoked in an arbitrary thread context
            handler.post(new Runnable() {
                @Override
                public void run() {
                    if (uri != null) {
                        Log.v(TAG, "Indexing completed for file '" + path + "', uri='" + uri + "'");
                        onWriteComplete(new File(path), uri);
                    } else {
                        Log.e(TAG, "Indexing failed for file '" + path + "'");
                        onWriteFailed();
                    }
                }
            });
        }
    };
}

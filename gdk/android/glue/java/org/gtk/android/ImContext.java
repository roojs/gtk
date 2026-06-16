package org.gtk.android;

import android.text.Editable;
import android.text.Selection;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.logging.Logger;

public final class ImContext {
	private static final String IME_LOG_TAG = "OLLMchat.IME";
	private static int deleteSeq = 0;
	private static long lastDeleteUptimeMs = 0;

	public static final class SurroundingRetVal {
		public String text;
		public int cursor_index;
		public int anchor_index;

		private SurroundingRetVal(String text, int cursor_idx, int anchor_idx) {
			this.text = text;
			this.cursor_index = cursor_idx;
			this.anchor_index = anchor_idx;
		}
	}

	private long native_ptr;
	public ImContext(long native_ptr) {
		this.native_ptr = native_ptr;
	}

	public native int getInputType();

	public native SurroundingRetVal getSurrounding();
	public native boolean deleteSurrounding(int offset, int n_chars);

	public native String getPreedit();
	public native void updatePreedit(String preedit, int cursor);

	public native boolean commit(String string);

	private void syncEditableFromGtk(Editable content, SurroundingRetVal surrounding) {
		if (content == null)
			return;
		if (surrounding == null || surrounding.text == null) {
			Log.i(IME_LOG_TAG, "syncEditableFromGtk: clearing editable (no surrounding from GTK)");
			content.clear();
			return;
		}
		Log.i(IME_LOG_TAG, "syncEditableFromGtk: gtk_len=" + surrounding.text.length()
				+ " cursor=" + surrounding.cursor_index
				+ " anchor=" + surrounding.anchor_index
				+ " editable_before=" + content.length());
		content.replace(0, content.length(), surrounding.text);
		int len = content.length();
		int cursor = Math.min(Math.max(surrounding.cursor_index, 0), len);
		int anchor = Math.min(Math.max(surrounding.anchor_index, 0), len);
		Selection.setSelection(content, anchor, cursor);
	}

	private static String surroundingSummary(SurroundingRetVal surrounding) {
		if (surrounding == null)
			return "null";
		if (surrounding.text == null)
			return "text=null";
		return "len=" + surrounding.text.length()
				+ " cursor=" + surrounding.cursor_index
				+ " anchor=" + surrounding.anchor_index;
	}

	private void syncEditableFromGtk(Editable content) {
		SurroundingRetVal surrounding = GlibContext.blockForMain(this::getSurrounding);
		syncEditableFromGtk(content, surrounding);
	}

	@Keep
	private static void reset(View view) {
		InputMethodManager imm = view.getContext().getSystemService(InputMethodManager.class);
		imm.restartInput(view);
	}

	final class ImeConnection extends BaseInputConnection {
		private Logger logger;

		public ImeConnection(@NonNull View target) {
			super(target, true);
			this.logger = Logger.getLogger("IME Connection");
		}

		@Override
		public boolean setComposingText(CharSequence text, int newCursorPosition) {
			logger.info("IME: setComposingText()");
			super.setComposingText(text, newCursorPosition);

			Editable content = getEditable();
			GlibContext.blockForMain(() -> {
				int a = Selection.getSelectionStart(content);
				int b = Selection.getSelectionEnd(content);
				if (a > b)
					b = a;

				ImContext.this.updatePreedit(content.toString(), b);
			});
			syncEditableFromGtk(getEditable());
			return true;
		}

		@Override
		public boolean finishComposingText() {
			logger.info("IME: finishComposingText()");
			super.finishComposingText();

			Editable content = getEditable();
			if (content.length() > 0)
				GlibContext.blockForMain(() -> ImContext.this.commit(content.toString()));
			syncEditableFromGtk(getEditable());
			return true;
		}

		@Override
		public boolean commitText(CharSequence text, int newCursorPosition) {
			logger.info("IME: commitText(\"" + text + "\", " + newCursorPosition + ")");

			if (text != null && text.length() > 0)
				GlibContext.blockForMain(() -> ImContext.this.commit(text.toString()));
			syncEditableFromGtk(getEditable());
			return true;
		}

		@Override
		public boolean deleteSurroundingText(int leftLength, int rightLength) {
			long now = android.os.SystemClock.uptimeMillis();
			long sinceLast = lastDeleteUptimeMs > 0 ? now - lastDeleteUptimeMs : -1;
			lastDeleteUptimeMs = now;
			int seq = ++deleteSeq;

			Editable editable = getEditable();
			SurroundingRetVal before = GlibContext.blockForMain(ImContext.this::getSurrounding);
			Log.i(IME_LOG_TAG, "deleteSurroundingText seq=" + seq
					+ " left=" + leftLength
					+ " right=" + rightLength
					+ " ms_since_last=" + sinceLast
					+ " editable_len=" + (editable != null ? editable.length() : -1)
					+ " gtk_before={" + surroundingSummary(before) + "}");

			GlibContext.blockForMain(() -> {
				if (leftLength > 0)
					ImContext.this.deleteSurrounding(-leftLength, leftLength);
				if (rightLength > 0)
					ImContext.this.deleteSurrounding(0, rightLength);
			});

			SurroundingRetVal after = GlibContext.blockForMain(ImContext.this::getSurrounding);
			Log.i(IME_LOG_TAG, "deleteSurroundingText seq=" + seq
					+ " done editable_len=" + (editable != null ? editable.length() : -1)
					+ " gtk_after={" + surroundingSummary(after) + "}");
			return true;
		}
	}
}

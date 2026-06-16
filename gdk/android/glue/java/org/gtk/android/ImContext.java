package org.gtk.android;

import android.text.Editable;
import android.text.Selection;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.logging.Logger;

public final class ImContext {
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
			content.clear();
			return;
		}
		content.replace(0, content.length(), surrounding.text);
		int len = content.length();
		int cursor = Math.min(Math.max(surrounding.cursor_index, 0), len);
		int anchor = Math.min(Math.max(surrounding.anchor_index, 0), len);
		Selection.setSelection(content, anchor, cursor);
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
			logger.info("IME: deleteSurroundingText(" + leftLength + ", " + rightLength + ")");

			GlibContext.blockForMain(() -> {
				if (leftLength > 0)
					ImContext.this.deleteSurrounding(-leftLength, leftLength);
				if (rightLength > 0)
					ImContext.this.deleteSurrounding(0, rightLength);
			});
			return true;
		}
	}
}

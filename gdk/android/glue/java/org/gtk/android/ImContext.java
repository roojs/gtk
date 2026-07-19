package org.gtk.android;

import android.text.Editable;
import android.text.Selection;
import android.view.KeyEvent;
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

	private void syncEditableFromGtk(Editable content) {
		SurroundingRetVal surrounding = GlibContext.blockForMain(this::getSurrounding);
		if (surrounding == null)
			return;

		content.replace(0, content.length(), surrounding.text);
		int len = content.length();
		int cursor = Math.min(Math.max(surrounding.cursor_index, 0), len);
		int anchor = Math.min(Math.max(surrounding.anchor_index, 0), len);
		Selection.setSelection(content, anchor, cursor);
	}

	@Keep
	private static void reset(View view) {
		/* Post to UI thread — same IMM vs blockForMain deadlock as setActiveImContext. */
		view.post(() -> {
			InputMethodManager imm = view.getContext().getSystemService(InputMethodManager.class);
			imm.restartInput(view);
		});
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

			final String preedit = text != null ? text.toString() : "";
			final int cursor = preedit.length();
			GlibContext.blockForMain(() -> {
				if (preedit.length() == 0)
					ImContext.this.updatePreedit(null, 0);
				else
					ImContext.this.updatePreedit(preedit, cursor);
			});
			return true;
		}

		@Override
		public boolean finishComposingText() {
			logger.info("IME: finishComposingText()");
			Editable content = getEditable();
			String toCommit = "";
			if (content != null) {
				int start = getComposingSpanStart(content);
				int end = getComposingSpanEnd(content);
				if (start >= 0 && end > start)
					toCommit = content.subSequence(start, end).toString();
			}

			super.finishComposingText();

			final String commit = toCommit;
			GlibContext.blockForMain(() -> {
				/* Android finishComposingText leaves text in place; only commit the
				 * composing span into GTK. Committing the whole Editable re-inserts
				 * already-committed text (autocomplete / focus-leave duplicates). */
				if (commit.length() > 0)
					ImContext.this.commit(commit);
				else
					ImContext.this.updatePreedit(null, 0);
			});
			syncEditableFromGtk(getEditable());
			return true;
		}

		@Override
		public boolean commitText(CharSequence text, int newCursorPosition) {
			logger.info("IME: commitText(\"" + text + "\", " + newCursorPosition + ")");

			if (text.length() > 0)
				GlibContext.blockForMain(() -> ImContext.this.commit(text.toString()));
			syncEditableFromGtk(getEditable());
			return true;
		}

		private void deleteBackwardOrSelection() {
			SurroundingRetVal surrounding = ImContext.this.getSurrounding();
			if (surrounding != null
					&& surrounding.cursor_index != surrounding.anchor_index) {
				int start = Math.min(surrounding.cursor_index, surrounding.anchor_index);
				int end = Math.max(surrounding.cursor_index, surrounding.anchor_index);
				int n = end - start;
				if (surrounding.cursor_index >= surrounding.anchor_index)
					ImContext.this.deleteSurrounding(-n, n);
				else
					ImContext.this.deleteSurrounding(0, n);
				return;
			}
			ImContext.this.deleteSurrounding(-1, 1);
		}

		@Override
		public boolean sendKeyEvent(KeyEvent event) {
			if (event.getKeyCode() != KeyEvent.KEYCODE_DEL)
				return super.sendKeyEvent(event);
			if (event.getAction() != KeyEvent.ACTION_DOWN
					&& event.getAction() != KeyEvent.ACTION_MULTIPLE)
				return super.sendKeyEvent(event);

			int count = event.getRepeatCount() > 0 ? event.getRepeatCount() : 1;
			for (int i = 0; i < count; i++)
				GlibContext.blockForMain(this::deleteBackwardOrSelection);
			return true;
		}

		@Override
		public boolean deleteSurroundingText(int leftLength, int rightLength) {
			logger.info("IME: deleteSurroundingText(" + leftLength + ", " + rightLength + ")");

			// The stock Samsung keyboard with 'Auto check spelling' enabled sends leftLength > 1.
			GlibContext.blockForMain(() -> {
				SurroundingRetVal surrounding = ImContext.this.getSurrounding();
				if (surrounding != null
						&& surrounding.cursor_index != surrounding.anchor_index) {
					deleteBackwardOrSelection();
					return;
				}
				if (leftLength > 0)
					ImContext.this.deleteSurrounding(-leftLength, leftLength);
				if (rightLength > 0)
					ImContext.this.deleteSurrounding(0, rightLength);
			});
			return true;
		}
	}
}

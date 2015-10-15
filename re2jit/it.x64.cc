void *        _compile (re2::Prog *) { return NULL; }
void          _destroy (void *) {}
rejit_entry_t _entry   (void *) { return NULL; }
bool          _run     (void *, struct rejit_threadset_t *, const char *) { return 0; }

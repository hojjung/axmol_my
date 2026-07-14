Module['preRun'] ||= [];
Module['preRun'].push(function() {
  addRunDependency('sync_writable_path');
  FS.mkdir('/axmolPersistPath');
  FS.mount(IDBFS, {}, '/axmolPersistPath');
  FS.syncfs(true, function(err) {
    if (err) {
      throw err;
    }
    removeRunDependency('sync_writable_path');
  });

  TTY.stream_ops.write = function(stream, buffer, offset, length, pos) {
    var tty = stream.tty;
    if (!tty || !tty.ops.put_char) {
      throw new FS.ErrnoError(60);
    }
    try {
      for (var i = 0; i < length; ++i) {
        var value = buffer[offset + i];
        if (value !== null && value !== 0) {
          tty.output.push(value);
        }
      }
    } catch (e) {
      throw new FS.ErrnoError(29);
    }
    out(UTF8ArrayToString(tty.output, 0));
    tty.output = [];
    if (length) {
      stream.node.timestamp = Date.now();
    }
    return i;
  };
});

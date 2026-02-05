if [ -d .git ]; then
  revh=$(git rev-list -1 --no-commit-header --date=format:%Y%m%d --format=%cd-%h HEAD 2>/dev/null || true)
  datecode=$(git rev-list -1 --no-commit-header --format=%ct HEAD 2>/dev/null || true)
  if [ -n "$revh" ] && [ -n "$datecode" ]; then
    echo "#define SERNO \"$revh\""
    echo "#define DATECODE ${datecode}UL"
    exit 0
  fi
fi
echo '#define SERNO "unknown"'
echo '#define DATECODE 0UL'


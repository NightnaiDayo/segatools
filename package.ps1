if ($null -eq $env:BUILD_DIR) {
    $BUILD_DIR="build"
    $BUILD_DIR_32="$BUILD_DIR\build32"
    $BUILD_DIR_64="$BUILD_DIR\build64"
    $BUILD_DIR_ZIP="$BUILD_DIR\zip"
    $DIST_DIR="dist"
    $DOC_DIR="doc"
} else {
    $BUILD_DIR = $env:BUILD_DIR;
    $BUILD_DIR_32 = $env:BUILD_DIR_32;
    $BUILD_DIR_64 = $env:BUILD_DIR_64;
    $BUILD_DIR_ZIP = $env:BUILD_DIR_ZIP;
    $DIST_DIR = $env:DIST_DIR;
    $DOC_DIR = $env:DOC_DIR;
}

$target = $null;
$line = '';
[System.Collections.ArrayList]$files = @();
$folder = Get-Location

cat .\Package.mk | % {
    $trimmed = $_.TrimEnd('\').
                  TrimStart("`t ").
                  Replace('$(V)', '').
                  Replace('$(BUILD_DIR)', $BUILD_DIR).
                  Replace('$(BUILD_DIR_32)', $BUILD_DIR_32).
                  Replace('$(BUILD_DIR_64)', $BUILD_DIR_64).
                  Replace('$(BUILD_DIR_ZIP)', $BUILD_DIR_ZIP).
                  Replace('$(DIST_DIR)', $DIST_DIR).
                  Replace('$(DOC_DIR)', $DOC_DIR).
                  Replace('$@', $target).
                  Replace('/', '\');

    if ($trimmed.EndsWith(': ') -or $trimmed.EndsWith(':')) {
        $target = $trimmed.TrimEnd(': ');
        $line = '';
        $files.Clear();
        cd $folder;

        return;
    }

    if (-not $trimmed.StartsWith('|')) {
        $line += $trimmed;

        if ($_.EndsWith('\')) {
            return;
        }
    }

    $line.Split(';') | % {
        $cmd = $_.Trim(' ');
        if ($cmd.StartsWith('echo')) {
            echo $cmd.TrimStart('echo ')
        } elseif ($cmd.StartsWith('mkdir')) {
            $cmd = $cmd.Replace('-p', '-Force')
            echo "    - $cmd"
            Invoke-Expression $cmd | Out-Null
        } elseif ($cmd.StartsWith('cp')) {
            $tokens = $cmd.Replace('cp ', '').Split(' ');
            $srcs = $tokens[0..($tokens.Count - 2)];
            $dest = $tokens[$tokens.Count - 1];
            echo "    - cp -Path $srcs -Destination $dest";
            cp -Path $srcs -Destination $dest
        } elseif ($cmd.StartsWith('cd')) {
            echo "    - $cmd"
            Invoke-Expression $cmd | Out-Null
        } elseif ($cmd.StartsWith('zip ')) {
            $tokens = $cmd.Substring(4, $cmd.Length - 4).Replace('-r ', '').Replace('-j ', '').Replace('*', '.\*');
            $tokens = $tokens.Split(' ');
            $target = $tokens[0]

            if (-not $tokens.Contains('$^')) {
                $files.AddRange($tokens[1..($tokens.Count - 1)]);
            }

            echo "    - Compress-Archive -Path $files -DestinationPath $target -Force"
            Compress-Archive -Path $files -DestinationPath $target -Force
        } else {
            $allExists = $true
            $args = $cmd.Split(' ');
            $args | ? { -not $_ -eq '' } | % {
                if (-not (Test-Path $_)) {
                    $allExists = $false
                }
            }

            if ($allExists) {
                $files.AddRange($args);
            }
        }
    }

    $line = '';
}

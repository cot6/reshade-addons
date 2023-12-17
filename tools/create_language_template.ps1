Param(
	[string]
	$locale = "en-US"
)

$ErrorActionPreference = 'Stop'
Get-Command xgettext.exe -CommandType Application | Out-Null

# This hash function has to match the one used in localization.hpp
function compute_crc16 {
	Param($data)

	$size = $data.Length
	$i = 0
	$crc = 0;
	while ($size--)
	{
		$crc = $crc -bxor $data[$i++]
		for ($k = 0; $k -lt 8; $k++) {
			$crc = if ($crc -band 1) { ($crc -shr 1) -bxor 0xa001 } else { $crc -shr 1 }
		}
	}
	return $crc
}

foreach ($proj in @( `
	@{Folder = "$(git rev-parse --show-toplevel)/src/addon-adjustdepth"; Include = @('dllmain.cpp')}, `
	@{Folder = "$(git rev-parse --show-toplevel)/src/addon-editorhistory"; Include = @('dllmain.cpp')}, `
	@{Folder = "$(git rev-parse --show-toplevel)/src/addon-screenshot"; Include = @('dllmain.cpp')}))
{
	$strings = ""
    $message = ""
	$is_inside_message = $false

	foreach ($line in xgettext.exe --c++ --keyword=_ --omit-header --indent --no-wrap --output=- $($proj.Include | ForEach-Object { $proj.Folder + '/' + $_ }))
    {
		if ($line.StartsWith("#") -or $line.Length -eq 0) {
			continue # Ignore comments
		}

		if ($line.StartsWith("msgid")) {
			$is_inside_message = $true
		}
		if ($line.StartsWith("msgstr")) {
			$hash = compute_crc16([System.Text.Encoding]::UTF8.GetBytes($message.Trim('"').Replace("\`"", "`"").Replace("\\", "\").Replace("\n", "`n")))

			$message = $message.Replace("\`"", "`"`"")
			$strings += "$hash $message`r`n"

			$message = ""
			$is_inside_message = $false
			continue
		}

		if ($is_inside_message) {
			if ($message.Length -ne 0) {
				$message = $message.Remove($message.Length - 1) + $line.Substring($line.IndexOf('"') + 1)
			} else {
				$message += $line.Substring($line.IndexOf('"'))
			}
		}
	}

	$ci = New-Object -TypeName System.Globalization.CultureInfo -ArgumentList $locale
	$locale = $ci.Name
	$locale_name = $ci.EnglishName
	$locale_short = $ci.ThreeLetterWindowsLanguageName
	$locale_lang_id = $ci.LCID -band 0x3FF
	$locale_sublang_id = ($ci.LCID -band 0xFC00) -shr 10

	Out-File -FilePath "$($proj.Folder)/res/lang_$locale.rc2" -Encoding ASCII -InputObject @"
/////////////////////////////////////////////////////////////////////////////
// $locale_name resources

#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_$locale_short)
LANGUAGE $locale_lang_id, $locale_sublang_id

/////////////////////////////////////////////////////////////////////////////
//
// String Table
//

STRINGTABLE
BEGIN

$strings
END

#endif    // $locale_name resources
/////////////////////////////////////////////////////////////////////////////
"@
}

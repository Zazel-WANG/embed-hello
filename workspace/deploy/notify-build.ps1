# 所有信息从 build-status.txt 读取（base64 编码），零命令行参数
$statusFile = "E:\AI-helper\projects\embed-hello\workspace\build\build-status.txt"

$status = "TEST"
$stage = ""
$commitMsg = ""

if (Test-Path $statusFile) {
    try {
        $b64 = (Get-Content $statusFile -Raw).Trim()
        $bytes = [Convert]::FromBase64String($b64)
        $text = [System.Text.Encoding]::UTF8.GetString($bytes)
        $lines = $text -split "`n" | ForEach-Object { $_.Trim() }
        if ($lines.Count -ge 1) { $status = $lines[0] }
        if ($lines.Count -ge 2 -and $lines[1] -ne "-") { $stage = $lines[1] }
        if ($lines.Count -ge 3) { $commitMsg = $lines[2] }
    } catch { }
}

if ($status -eq "SUCCESS") {
    $title = "CI/CD Passed"
    $body  = if ($commitMsg) { $commitMsg } else { "All stages passed" }
} else {
    $title = "CI/CD FAILED"
    if ($stage -and $commitMsg) {
        $body = "[$stage] $commitMsg"
    } elseif ($commitMsg) {
        $body = $commitMsg
    } else {
        $body = "Build failed"
    }
}

[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] | Out-Null

$template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent(
    [Windows.UI.Notifications.ToastTemplateType]::ToastText02
)
$nodes = @($template.GetElementsByTagName('text'))
$null = $nodes[0].AppendChild($template.CreateTextNode($title))
$null = $nodes[1].AppendChild($template.CreateTextNode($body))

$toast = [Windows.UI.Notifications.ToastNotification]::new($template)
[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('Claude CI/CD').Show($toast)

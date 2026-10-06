param(
    [string]$Exe
)
# Sign the updater so Windows trusts it (SmartScreen + Smart App Control pass).
#
# Two ways to fill this in:
#
# 1) Classic cert: export your .pfx + password here.
#    signtool sign /f "C:\path\cert.pfx" /p "PASSWORD" /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $Exe
#
# 2) Azure Trusted Signing (~$9.99/mo, Microsoft's own service):
#    sign with the AzureSignTool or the Trusted Signing SDK, e.g.
#    AzureSignTool sign -kvu "<vault-uri>" -kvi "<client-id>" -kvs "<client-secret>" `
#        -kvc "<cert-name>" -tr http://timestamp.acs.microsoft.com -v $Exe
#
# The signed file replaces $Exe. Build.ps1 calls this automatically when the
# file exists.
Write-Host "SKIP: sign.ps1 is a template - add your signing credentials to sign"
exit 0
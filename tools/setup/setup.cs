using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Net;
using System.Reflection;
using System.Threading.Tasks;
using System.Windows.Forms;

[assembly: AssemblyTitle("Rezo Setup")]
[assembly: AssemblyProduct("Rezo")]
[assembly: AssemblyCompany("Pandajupiter")]
[assembly: AssemblyDescription("Rezo privacy browser - installer")]
[assembly: AssemblyCopyright("Copyright (c) Pandajupiter")]
[assembly: AssemblyVersion("1.5.4.0")]
[assembly: AssemblyFileVersion("1.5.4.0")]

class RezoSetupForm : Form
{
    const string APP_VERSION = "1.5.4";
    const string DEFAULT_SERVER = "https://github.com/0Panda-Development/rezo/releases/latest/download/";

    static readonly string ExeDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);

    // UI Components
    Panel mainPanel;
    PictureBox logoBox;
    Label titleLabel;
    Label subtitleLabel;
    Label versionLabel;
    Panel progressPanel;
    Label progressLabel;
    Panel progressBarBg;
    Panel progressBarFill;
    Label statusLabel;
    Label detailLabel;
    Label speedLabel;
    Timer animationTimer;
    Timer progressTimer;
    float animationProgress = 0f;
    int currentProgress = 0;
    int targetProgress = 0;

    // Colors
    static readonly Color BG_DARK = Color.FromArgb(11, 15, 26);
    static readonly Color BG_CARD = Color.FromArgb(22, 35, 58);
    static readonly Color ACCENT_CYAN = Color.FromArgb(79, 227, 255);
    static readonly Color ACCENT_DIM = Color.FromArgb(40, 120, 140);
    static readonly Color TEXT_WHITE = Color.FromArgb(230, 234, 242);
    static readonly Color TEXT_DIM = Color.FromArgb(139, 147, 167);
    static readonly Color SUCCESS_GREEN = Color.FromArgb(52, 211, 153);
    static readonly Color WARN_ORANGE = Color.FromArgb(245, 158, 11);
    static readonly Color ERROR_RED = Color.FromArgb(248, 113, 113);

    [STAThread]
    static void Main()
    {
        if (!IsRunAsAdmin())
        {
            var startInfo = new ProcessStartInfo(Assembly.GetExecutingAssembly().Location)
            {
                UseShellExecute = true,
                Verb = "runas"
            };
            try { Process.Start(startInfo); } catch { }
            return;
        }

        ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new RezoSetupForm());
    }

    static bool IsRunAsAdmin()
    {
        using (var identity = System.Security.Principal.WindowsIdentity.GetCurrent())
        {
            var principal = new System.Security.Principal.WindowsPrincipal(identity);
            return principal.IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
        }
    }

    RezoSetupForm()
    {
        Text = "Rezo Setup";
        FormBorderStyle = FormBorderStyle.None;
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = true;
        ClientSize = new Size(560, 420);
        BackColor = BG_DARK;
        StartPosition = FormStartPosition.CenterScreen;
        DoubleBuffered = true;

        try { Icon = new Icon(Assembly.GetExecutingAssembly().GetManifestResourceStream("logo.ico")); } catch { }

        BuildUI();
        Shown += async (s, e) => await RunInstall();
    }

    void BuildUI()
    {
        // Main panel with rounded corners
        mainPanel = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.Transparent,
        };
        mainPanel.Paint += (s, e) => DrawRoundedRect(e.Graphics, mainPanel.ClientRectangle, 16, BG_DARK);
        Controls.Add(mainPanel);

        // Logo
        logoBox = new PictureBox
        {
            SizeMode = PictureBoxSizeMode.Zoom,
            Size = new Size(100, 100),
            Location = new Point(230, 30),
            BackColor = Color.Transparent,
        };
        try { logoBox.Image = Image.FromStream(Assembly.GetExecutingAssembly().GetManifestResourceStream("logo.png")); } catch { }
        mainPanel.Controls.Add(logoBox);

        // Title
        titleLabel = new Label
        {
            Text = "Rezo",
            Font = new Font("Segoe UI", 32, FontStyle.Bold),
            ForeColor = Color.FromArgb(230, 234, 242),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 145),
            TextAlign = ContentAlignment.MiddleCenter,
        };
        titleLabel.Width = mainPanel.Width;
        mainPanel.Controls.Add(titleLabel);

        // Subtitle
        subtitleLabel = new Label
        {
            Text = "Privacy Browser",
            Font = new Font("Segoe UI", 12, FontStyle.Regular),
            ForeColor = Color.FromArgb(139, 147, 167),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 190),
            TextAlign = ContentAlignment.MiddleCenter,
        };
        subtitleLabel.Width = mainPanel.Width;
        mainPanel.Controls.Add(subtitleLabel);

        // Version
        versionLabel = new Label
        {
            Text = "Version " + APP_VERSION,
            Font = new Font("Segoe UI", 10, FontStyle.Regular),
            ForeColor = Color.FromArgb(40, 120, 140),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 218),
            TextAlign = ContentAlignment.MiddleCenter,
        };
        versionLabel.Width = mainPanel.Width;
        mainPanel.Controls.Add(versionLabel);

        // Progress panel
        progressPanel = new Panel
        {
            Size = new Size(420, 120),
            Location = new Point(70, 260),
            BackColor = Color.Transparent,
        };
        mainPanel.Controls.Add(progressPanel);

        progressLabel = new Label
        {
            Text = "Preparing installation...",
            Font = new Font("Segoe UI", 11, FontStyle.Regular),
            ForeColor = Color.FromArgb(230, 234, 242),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 0),
        };
        progressPanel.Controls.Add(progressLabel);

        // Progress bar background
        progressBarBg = new Panel
        {
            Size = new Size(420, 10),
            Location = new Point(0, 40),
            BackColor = Color.FromArgb(22, 35, 58),
        };
        progressBarBg.Paint += (s, e) => DrawRoundedRect(e.Graphics, progressBarBg.ClientRectangle, 5, Color.FromArgb(22, 35, 58));
        progressPanel.Controls.Add(progressBarBg);

        // Progress bar fill
        progressBarFill = new Panel
        {
            Size = new Size(0, 10),
            Location = new Point(0, 0),
            BackColor = Color.FromArgb(79, 227, 255),
        };
        progressBarFill.Paint += (s, e) => DrawRoundedRect(e.Graphics, progressBarFill.ClientRectangle, 5, Color.FromArgb(79, 227, 255));
        progressBarBg.Controls.Add(progressBarFill);

        // Status
        statusLabel = new Label
        {
            Text = "",
            Font = new Font("Segoe UI", 9, FontStyle.Regular),
            ForeColor = Color.FromArgb(139, 147, 167),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 55),
        };
        progressPanel.Controls.Add(statusLabel);

        // Detail
        detailLabel = new Label
        {
            Text = "",
            Font = new Font("Segoe UI", 8, FontStyle.Regular),
            ForeColor = Color.FromArgb(100, 110, 130),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 75),
            MaximumSize = new Size(420, 40),
        };
        progressPanel.Controls.Add(detailLabel);

        // Speed
        speedLabel = new Label
        {
            Text = "",
            Font = new Font("Segoe UI", 8, FontStyle.Regular),
            ForeColor = Color.FromArgb(79, 227, 255),
            BackColor = Color.Transparent,
            AutoSize = true,
            Location = new Point(0, 95),
        };
        progressPanel.Controls.Add(speedLabel);

        // Animation timer
        animationTimer = new Timer { Interval = 30 };
        animationTimer.Tick += (s, e) =>
        {
            animationProgress += 0.02f;
            if (animationProgress > 1.5f) animationProgress = -0.5f;
            UpdateIndeterminateProgress();
        };

        progressTimer = new Timer { Interval = 30 };
        progressTimer.Tick += (s, e) => AnimateProgressBar();
    }

    void DrawRoundedRect(Graphics g, Rectangle rect, int radius, Color color)
    {
        using (var path = GetRoundedRectPath(rect, radius))
        using (var brush = new SolidBrush(color))
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.FillPath(brush, path);
        }
    }

    GraphicsPath GetRoundedRectPath(Rectangle rect, int radius)
    {
        var path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        // Subtle glow border
        using (var pen = new Pen(Color.FromArgb(30, 79, 227, 255), 1))
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.DrawPath(pen, GetRoundedRectPath(new Rectangle(1, 1, Width - 3, Height - 3), 16));
        }
    }

    void UpdateIndeterminateProgress()
    {
        if (progressBarFill.InvokeRequired)
        {
            progressBarFill.Invoke(new Action(UpdateIndeterminateProgress));
            return;
        }

        int barWidth = progressBarBg.Width;
        int fillWidth = (int)(barWidth * 0.3f);
        int x = (int)((barWidth - fillWidth) * Math.Max(0, animationProgress));
        progressBarFill.Width = fillWidth;
        progressBarFill.Left = x;
    }

    void SetProgress(int percent, string status = "", string detail = "", string speed = "")
    {
        if (InvokeRequired)
        {
            Invoke(new Action<int, string, string, string>(SetProgress), percent, status, detail, speed);
            return;
        }

        targetProgress = Math.Max(0, Math.Min(100, percent));
        if (!string.IsNullOrEmpty(status)) progressLabel.Text = status;
        if (!string.IsNullOrEmpty(detail)) detailLabel.Text = detail;
        if (!string.IsNullOrEmpty(speed)) speedLabel.Text = speed;

        if (!progressTimer.Enabled) progressTimer.Start();
    }

    void AnimateProgressBar()
    {
        if (currentProgress < targetProgress)
        {
            currentProgress = Math.Min(targetProgress, currentProgress + 3);
            progressBarFill.Width = (int)(progressBarBg.Width * currentProgress / 100f);
        }
        else if (currentProgress > targetProgress)
        {
            currentProgress = Math.Max(targetProgress, currentProgress - 3);
            progressBarFill.Width = (int)(progressBarBg.Width * currentProgress / 100f);
        }
        else
        {
            progressTimer.Stop();
        }
    }

    async Task RunInstall()
    {
        animationTimer.Start();
        SetProgress(0, "Initializing...", "Connecting to update server...");

        try
        {
            string server = DEFAULT_SERVER;
            SetProgress(10, "Connecting to server...", "Fetching latest version...");

            string ver = await Task.Run(() => Fetch(server + "version.txt"));
            if (ver == null)
            {
                ShowError("Update server unreachable", "Please check your internet connection");
                return;
            }

            SetProgress(20, "Downloading installer...", "Version " + ver, "");
            string msixName = "Rezo-" + ver + ".msix";
            string msixPath = Path.Combine(Path.GetTempPath(), "rezo-setup", msixName);
            string cerPath = Path.Combine(Path.GetTempPath(), "rezo-setup", "Rezo-selfsign.cer");
            Directory.CreateDirectory(Path.GetDirectoryName(msixPath));

            string msixUrl = server + msixName;
            string cerUrl = server + "Rezo-selfsign.cer";

            // Download MSIX with progress
            await Task.Run(() => DownloadWithProgress(msixUrl, msixPath, (p, s, sp) => 
                SetProgress(20 + p * 60 / 100, "Downloading Rezo " + ver + "...", s, sp)));

            // Download certificate
            await Task.Run(() => Download(cerUrl, cerPath));

            SetProgress(85, "Installing certificate...", "Trusting Rezo publisher...", "");
            InstallCertificate(cerPath);

            SetProgress(90, "Enabling sideloading...", "Configuring Windows...", "");
            EnableSideloading();

            SetProgress(95, "Installing Rezo...", "This may take a moment...", "");
            await Task.Run(() => InstallMSIX(msixPath));

            SetProgress(100, "Installation complete!", "Launching Rezo...", "");
            progressBarFill.BackColor = Color.FromArgb(52, 211, 153);
            animationTimer.Stop();
            progressTimer.Stop();

            await Task.Delay(800);
            LaunchRezo();
        }
        catch (Exception ex)
        {
            ShowError("Installation failed", ex.Message);
        }
    }

    void DownloadWithProgress(string url, string dest, Action<int, string, string> progressCallback)
    {
        var req = (HttpWebRequest)WebRequest.Create(url);
        req.Timeout = 600000;
        req.ReadWriteTimeout = 600000;
        req.UserAgent = "RezoSetup/" + APP_VERSION;

        using (var resp = (HttpWebResponse)req.GetResponse())
        using (var src = resp.GetResponseStream())
        using (var dst = File.Create(dest))
        {
            long total = resp.ContentLength;
            long read = 0;
            byte[] buffer = new byte[81920];
            int bytesRead;
            var sw = System.Diagnostics.Stopwatch.StartNew();
            while ((bytesRead = src.Read(buffer, 0, buffer.Length)) > 0)
            {
                dst.Write(buffer, 0, bytesRead);
                read += bytesRead;
                if (total > 0)
                {
                    int pct = (int)(read * 100 / total);
                    double mbRead = read / 1024.0 / 1024.0;
                    double mbTotal = total / 1024.0 / 1024.0;
                    double speed = mbRead / sw.Elapsed.TotalSeconds;
                    progressCallback(pct, string.Format("{0:F1} / {1:F1} MB", mbRead, mbTotal), string.Format("{0:F1} MB/s", speed));
                }
            }
        }
    }

    void Download(string url, string dest)
    {
        var req = (HttpWebRequest)WebRequest.Create(url);
        req.Timeout = 600000;
        req.ReadWriteTimeout = 600000;
        req.UserAgent = "RezoSetup/" + APP_VERSION;
        using (var resp = (HttpWebResponse)req.GetResponse())
        using (var src = resp.GetResponseStream())
        using (var dst = File.Create(dest))
            src.CopyTo(dst);
    }

    string Fetch(string url)
    {
        try
        {
            var req = (HttpWebRequest)WebRequest.Create(url);
            req.Timeout = 8000;
            req.ReadWriteTimeout = 8000;
            req.UserAgent = "RezoSetup";
            using (var resp = (HttpWebResponse)req.GetResponse())
            using (var r = new StreamReader(resp.GetResponseStream()))
                return r.ReadToEnd().Trim();
        }
        catch { return null; }
    }

    void InstallCertificate(string cerPath)
    {
        try
        {
            var store = new System.Security.Cryptography.X509Certificates.X509Store(
                System.Security.Cryptography.X509Certificates.StoreName.Root,
                System.Security.Cryptography.X509Certificates.StoreLocation.LocalMachine);
            store.Open(System.Security.Cryptography.X509Certificates.OpenFlags.ReadWrite);
            store.Add(new System.Security.Cryptography.X509Certificates.X509Certificate2(cerPath));
            store.Close();
        }
        catch (Exception e)
        {
            throw new Exception("Certificate installation failed: " + e.Message);
        }
    }

    void EnableSideloading()
    {
        try
        {
            Microsoft.Win32.Registry.LocalMachine.CreateSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock")
                .SetValue("AllowAllTrustedApps", 1, Microsoft.Win32.RegistryValueKind.DWord);
        }
        catch (Exception e)
        {
            throw new Exception("Sideloading enable failed: " + e.Message);
        }
    }

    void InstallMSIX(string msixPath)
    {
        var p = new Process
        {
            StartInfo = new ProcessStartInfo("powershell",
                "-NoProfile -Command \"Add-AppxPackage -Path '" + msixPath + "' -ForceApplicationShutdown\"")
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
            }
        };
        p.Start();
        p.WaitForExit();
        string err = p.StandardError.ReadToEnd();
        if (p.ExitCode != 0 && !err.Contains("already installed"))
        {
            throw new Exception("Install failed: " + err);
        }
    }

    void LaunchRezo()
    {
        try
        {
            var ps = new Process
            {
                StartInfo = new ProcessStartInfo("powershell",
                    "-NoProfile -Command \"$p=Get-AppxPackage Pandajupiter.Rezo; explorer ('shell:AppsFolder\\' + $p.PackageFamilyName + '!Rezo')\"")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                }
            };
            ps.Start();
        }
        catch { }
        Close();
    }

    void ShowError(string title, string msg)
    {
        if (InvokeRequired)
        {
            Invoke(new Action<string, string>(ShowError), title, msg);
            return;
        }
        animationTimer.Stop();
        progressTimer.Stop();
        progressBarFill.BackColor = Color.FromArgb(248, 113, 113);
        progressBarBg.BackColor = Color.FromArgb(60, 20, 20);
        progressLabel.Text = title;
        progressLabel.ForeColor = Color.FromArgb(248, 113, 113);
        detailLabel.Text = msg;
        speedLabel.Text = "";
        progressLabel.Font = new Font("Segoe UI", 12, FontStyle.Bold);
    }
}
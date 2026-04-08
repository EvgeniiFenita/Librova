using Librova.UI.Logging;
using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal sealed class ConverterValidationCoordinator : IDisposable
{
    private readonly Func<string, CancellationToken, Task<Fb2ProbeResult>> _converterProbe;
    private CancellationTokenSource? _converterProbeCts;
    private bool _isDisposed;

    public ConverterValidationCoordinator(Func<string, CancellationToken, Task<Fb2ProbeResult>>? converterProbe = null)
    {
        _converterProbe = converterProbe ?? Fb2ConverterProbe.RunAsync;
    }

    public event Action? StateChanged;

    public string ValidationMessage { get; private set; } = string.Empty;
    public bool IsProbeInProgress { get; private set; }
    public Task CurrentProbeTask { get; private set; } = Task.CompletedTask;

    public void Initialize(string executablePath)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        CancelCurrentProbe();
        ValidationMessage = BuildValidationMessage(executablePath);
        IsProbeInProgress = false;
        CurrentProbeTask = Task.CompletedTask;
        StateChanged?.Invoke();
    }

    public void ScheduleValidation(string executablePath)
    {
        ObjectDisposedException.ThrowIf(_isDisposed, this);
        CancelCurrentProbe();

        var syncMessage = BuildValidationMessage(executablePath);
        if (!string.IsNullOrEmpty(syncMessage) || string.IsNullOrWhiteSpace(executablePath))
        {
            ValidationMessage = syncMessage;
            IsProbeInProgress = false;
            CurrentProbeTask = Task.CompletedTask;
            StateChanged?.Invoke();
            return;
        }

        var cts = new CancellationTokenSource();
        _converterProbeCts = cts;
        ValidationMessage = string.Empty;
        IsProbeInProgress = true;
        CurrentProbeTask = RunProbeAsync(executablePath, cts.Token);
        StateChanged?.Invoke();
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        CancelCurrentProbe();
        IsProbeInProgress = false;
        CurrentProbeTask = Task.CompletedTask;
        _isDisposed = true;
    }

    private async Task RunProbeAsync(string executablePath, CancellationToken cancellationToken)
    {
        try
        {
            await Task.Delay(500, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return;
        }

        Fb2ProbeResult result;
        try
        {
            UiLogging.Information("Running converter probe. Executable={Executable}", executablePath);
            result = await _converterProbe(executablePath, cancellationToken);
        }
        catch (OperationCanceledException)
        {
            return;
        }
        catch (Exception ex)
        {
            UiLogging.Warning("Converter probe threw unexpected exception. {Error}", ex.Message);
            result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ProbeError };
        }

        if (cancellationToken.IsCancellationRequested)
        {
            return;
        }

        IsProbeInProgress = false;
        ValidationMessage = result.BuildValidationMessage();
        UiLogging.Information(
            "Converter probe completed. Outcome={Outcome} Executable={Executable}",
            result.Outcome,
            executablePath);
        StateChanged?.Invoke();
    }

    private void CancelCurrentProbe()
    {
        _converterProbeCts?.Cancel();
        _converterProbeCts?.Dispose();
        _converterProbeCts = null;
    }

    private static string BuildValidationMessage(string executablePath)
    {
        if (string.IsNullOrWhiteSpace(executablePath))
        {
            return string.Empty;
        }

        if (!Path.IsPathFullyQualified(executablePath))
        {
            return "Built-in fb2cng executable path must be absolute.";
        }

        return string.Empty;
    }
}

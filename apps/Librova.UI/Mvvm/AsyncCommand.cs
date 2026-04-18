using System;
using System.Threading.Tasks;
using System.Windows.Input;
using Librova.UI.Logging;

namespace Librova.UI.Mvvm;

internal sealed class AsyncCommand : ICommand
{
    private readonly Func<Task> _execute;
    private readonly Func<bool>? _canExecute;
    private readonly Func<Exception, Task>? _onError;
    private bool _isRunning;

    public AsyncCommand(
        Func<Task> execute,
        Func<bool>? canExecute = null,
        Func<Exception, Task>? onError = null)
    {
        _execute = execute;
        _canExecute = canExecute;
        _onError = onError;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => !_isRunning && (_canExecute?.Invoke() ?? true);

    public async void Execute(object? parameter)
    {
        await ExecuteAsync(parameter);
    }

    public Task ExecuteAsyncForTests() => ExecuteAsync(null);

    private async Task ExecuteAsync(object? parameter)
    {
        if (!CanExecute(parameter))
        {
            return;
        }

        _isRunning = true;
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);

        try
        {
            await _execute();
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Async command execution failed. CommandType={CommandType}", GetType().Name);
            if (_onError is not null)
            {
                try
                {
                    await _onError(error);
                }
                catch (Exception handlerError)
                {
                    UiLogging.Error(
                        handlerError,
                        "Async command error handler failed. CommandType={CommandType}",
                        GetType().Name);
                }
            }
        }
        finally
        {
            _isRunning = false;
            CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}

internal sealed class AsyncCommand<T> : ICommand
{
    private readonly Func<T?, Task> _execute;
    private readonly Func<T?, bool>? _canExecute;
    private readonly Func<Exception, Task>? _onError;
    private bool _isRunning;

    public AsyncCommand(
        Func<T?, Task> execute,
        Func<T?, bool>? canExecute = null,
        Func<Exception, Task>? onError = null)
    {
        _execute = execute;
        _canExecute = canExecute;
        _onError = onError;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => !_isRunning && (_canExecute?.Invoke((T?)parameter) ?? true);

    public async void Execute(object? parameter)
    {
        await ExecuteAsync(parameter);
    }

    public Task ExecuteAsyncForTests(T? parameter) => ExecuteAsync(parameter);

    private async Task ExecuteAsync(object? parameter)
    {
        if (!CanExecute(parameter))
        {
            return;
        }

        _isRunning = true;
        CanExecuteChanged?.Invoke(this, EventArgs.Empty);

        try
        {
            await _execute((T?)parameter);
        }
        catch (Exception error)
        {
            UiLogging.Error(error, "Async command execution failed. CommandType={CommandType}", GetType().Name);
            if (_onError is not null)
            {
                try
                {
                    await _onError(error);
                }
                catch (Exception handlerError)
                {
                    UiLogging.Error(
                        handlerError,
                        "Async command error handler failed. CommandType={CommandType}",
                        GetType().Name);
                }
            }
        }
        finally
        {
            _isRunning = false;
            CanExecuteChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}

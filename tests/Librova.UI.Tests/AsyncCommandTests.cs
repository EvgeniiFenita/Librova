using Librova.UI.Mvvm;
using Xunit;

namespace Librova.UI.Tests;

public sealed class AsyncCommandTests
{
    [Fact]
    public async Task AsyncCommand_InvokesErrorHandlerInsteadOfThrowing()
    {
        string? capturedMessage = null;
        var command = new AsyncCommand(
            () => throw new InvalidOperationException("boom"),
            onError: error =>
            {
                capturedMessage = error.Message;
                return Task.CompletedTask;
            });

        await command.ExecuteAsyncForTests();

        Assert.Equal("boom", capturedMessage);
        Assert.True(command.CanExecute(null));
    }

    [Fact]
    public async Task AsyncCommand_SwallowsExceptionsThrownByErrorHandler()
    {
        var command = new AsyncCommand(
            () => throw new InvalidOperationException("boom"),
            onError: _ => throw new InvalidOperationException("handler failed"));

        await command.ExecuteAsyncForTests();

        Assert.True(command.CanExecute(null));
    }
}


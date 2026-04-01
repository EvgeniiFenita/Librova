using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
using Xunit;

namespace Librova.UI.Tests;

public sealed class StrongIntegrationTests
{
    [Fact]
    public async Task ShellApplication_InitializeAsync_PreloadsExistingBooksThroughRealHost()
    {
        var sandboxRoot = CreateSandboxRoot("shell-preload");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "ShellPreload");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);

            await ImportBookAsync(
                session.ImportJobs,
                sandboxRoot,
                "monday.fb2",
                "Monday Begins on Saturday",
                "Arkady",
                "Strugatsky",
                cancellation.Token);

            await using var application = CreateApplication(session, sandboxRoot);
            await application.InitializeAsync();

            Assert.Single(application.Shell.LibraryBrowser.Books);
            Assert.Equal("Monday Begins on Saturday", application.Shell.LibraryBrowser.Books[0].Title);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task ShellApplication_ImportCommand_RefreshesBrowserThroughRealHost()
    {
        var sandboxRoot = CreateSandboxRoot("shell-import-refresh");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "ShellImportRefresh");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);
            await using var application = CreateApplication(session, sandboxRoot);

            await application.InitializeAsync();
            Assert.Empty(application.Shell.LibraryBrowser.Books);

            application.Shell.ImportJobs.SourcePath = CreateFb2File(
                sandboxRoot,
                "fresh-import.fb2",
                "Fresh Import",
                "Boris",
                "Strugatsky");
            application.Shell.ImportJobs.WorkingDirectory = Path.Combine(sandboxRoot, "UiWork");

            await application.Shell.ImportJobs.StartImportCommand.ExecuteAsyncForTests();

            Assert.Single(application.Shell.LibraryBrowser.Books);
            Assert.Equal("Fresh Import", application.Shell.LibraryBrowser.Books[0].Title);
            Assert.Equal("Completed", application.Shell.ImportJobs.LastResult?.Snapshot.Status.ToString());
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task ShellApplication_AllowsStrictDuplicateImportAsIndependentBookWhenEnabled()
    {
        var sandboxRoot = CreateSandboxRoot("shell-strict-duplicate-override");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "ShellStrictDuplicateOverride");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);
            await using var application = CreateApplication(session, sandboxRoot);

            await application.InitializeAsync();

            var firstSource = CreateFb2File(
                sandboxRoot,
                "duplicate-one.fb2",
                "Duplicated Book",
                "Arkady",
                "Strugatsky",
                "978-5-17-090334-4");
            var secondSource = CreateFb2File(
                sandboxRoot,
                "duplicate-two.fb2",
                "Duplicated Book",
                "Arkady",
                "Strugatsky",
                "978-5-17-090334-4");

            application.Shell.ImportJobs.SourcePath = firstSource;
            application.Shell.ImportJobs.WorkingDirectory = Path.Combine(sandboxRoot, "UiWork", "first");
            await application.Shell.ImportJobs.StartImportAsync();

            application.Shell.ImportJobs.SourcePath = secondSource;
            application.Shell.ImportJobs.WorkingDirectory = Path.Combine(sandboxRoot, "UiWork", "second");
            application.Shell.ImportJobs.AllowProbableDuplicates = true;
            await application.Shell.ImportJobs.StartImportAsync();

            await application.Shell.LibraryBrowser.RefreshAsync();

            Assert.Equal(2, application.Shell.LibraryBrowser.Books.Count);
            Assert.All(application.Shell.LibraryBrowser.Books, book => Assert.Equal("Duplicated Book", book.Title));
            Assert.Equal(2, application.Shell.LibraryBrowser.Books.Select(book => book.BookId).Distinct().Count());
            Assert.Equal(2, application.Shell.LibraryBrowser.Books.Select(book => book.ManagedPath).Distinct().Count());
            Assert.Contains("Completed", application.Shell.ImportJobs.StatusText, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_PaginatesAgainstRealHost()
    {
        var sandboxRoot = CreateSandboxRoot("browser-paging");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "BrowserPaging");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);

            await ImportBookAsync(session.ImportJobs, sandboxRoot, "alpha.fb2", "Alpha", "Author", "One", cancellation.Token);
            await ImportBookAsync(session.ImportJobs, sandboxRoot, "beta.fb2", "Beta", "Author", "Two", cancellation.Token);
            await ImportBookAsync(session.ImportJobs, sandboxRoot, "gamma.fb2", "Gamma", "Author", "Three", cancellation.Token);

            var viewModel = new LibraryBrowserViewModel(session.LibraryCatalog, libraryRoot: options.LibraryRoot)
            {
                PageSize = 2
            };

            await viewModel.RefreshAsync();
            Assert.Equal(2, viewModel.Books.Count);
            Assert.True(viewModel.HasMoreResults);
            Assert.Equal("Page 1+", viewModel.PageLabelText);

            await viewModel.NextPageAsync();
            Assert.Single(viewModel.Books);
            Assert.False(viewModel.HasMoreResults);
            Assert.Equal(2, viewModel.CurrentPage);
            Assert.Equal("Page 2", viewModel.PageLabelText);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ExportsSelectedBookAgainstRealHost()
    {
        var sandboxRoot = CreateSandboxRoot("browser-export");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "BrowserExport");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);

            await ImportBookAsync(session.ImportJobs, sandboxRoot, "export.fb2", "Exported Book", "Arkady", "Strugatsky", cancellation.Token);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportedBook.fb2");
            var viewModel = new LibraryBrowserViewModel(
                session.LibraryCatalog,
                new FakeExportSelectionService(exportPath),
                options.LibraryRoot);

            await viewModel.RefreshAsync();
            await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

            await viewModel.ExportSelectedBookAsync();

            Assert.True(File.Exists(exportPath));
            Assert.Contains("Exported", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_MovesSelectedBookToTrashAgainstRealHost()
    {
        var sandboxRoot = CreateSandboxRoot("browser-trash");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = CreateHostOptions(sandboxRoot, "BrowserTrash");
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await using var session = await ShellBootstrap.StartSessionAsync(options, cancellation.Token);

            await ImportBookAsync(session.ImportJobs, sandboxRoot, "trash.fb2", "Trash Book", "Arkady", "Strugatsky", cancellation.Token);

            var viewModel = new LibraryBrowserViewModel(session.LibraryCatalog, libraryRoot: options.LibraryRoot);

            await viewModel.RefreshAsync();
            await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

            await viewModel.MoveSelectedBookToTrashAsync();

            Assert.Empty(viewModel.Books);
            Assert.Contains("Moved", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            TryDeleteDirectory(sandboxRoot);
        }
    }

    private static ShellApplication CreateApplication(ShellSession session, string sandboxRoot) =>
        ShellApplication.Create(
            session,
            stateStore: new ShellStateStore(Path.Combine(sandboxRoot, "ui-shell-state.json")),
            preferencesStore: new UiPreferencesStore(Path.Combine(sandboxRoot, "ui-preferences.json")));

    private static CoreHostLaunchOptions CreateHostOptions(string sandboxRoot, string suffix) =>
        new()
        {
            ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                AppContext.BaseDirectory,
                "..",
                "..",
                "..",
                "..")),
            PipePath = $@"\\.\pipe\Librova.UI.StrongTests.{suffix}.{Environment.ProcessId}.{Environment.TickCount64}",
            LibraryRoot = Path.Combine(sandboxRoot, "Library")
        };

    private static string CreateSandboxRoot(string scenario) =>
        Path.Combine(Path.GetTempPath(), "librova-ui-strong-tests", scenario, Guid.NewGuid().ToString("N"));

    private static async Task ImportBookAsync(
        IImportJobsService importJobs,
        string sandboxRoot,
        string fileName,
        string title,
        string authorFirstName,
        string authorLastName,
        CancellationToken cancellationToken)
    {
        var sourcePath = CreateFb2File(sandboxRoot, fileName, title, authorFirstName, authorLastName);
        var workDirectory = Path.Combine(sandboxRoot, "Work", Path.GetFileNameWithoutExtension(fileName));
        var jobId = await importJobs.StartAsync(
            new StartImportRequestModel
            {
                SourcePaths = [sourcePath],
                WorkingDirectory = workDirectory
            },
            TimeSpan.FromSeconds(5),
            cancellationToken);

        var completed = await importJobs.WaitAsync(
            jobId,
            TimeSpan.FromSeconds(5),
            TimeSpan.FromSeconds(5),
            cancellationToken);
        Assert.True(completed);

        var result = await importJobs.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellationToken);
        Assert.NotNull(result);
        Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
        Assert.NotNull(result.Summary);
        Assert.Equal(1UL, result.Summary!.ImportedEntries);
    }

    private static string CreateFb2File(
        string sandboxRoot,
        string fileName,
        string title,
        string authorFirstName,
        string authorLastName,
        string? isbn = null)
    {
        var sourcePath = Path.Combine(sandboxRoot, fileName);
        File.WriteAllText(
            sourcePath,
            $$"""
            <?xml version="1.0" encoding="UTF-8"?>
            <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
              <description>
                <title-info>
                  <book-title>{{title}}</book-title>
                  <author>
                    <first-name>{{authorFirstName}}</first-name>
                    <last-name>{{authorLastName}}</last-name>
                  </author>
                  <lang>en</lang>
                </title-info>
                {{(string.IsNullOrWhiteSpace(isbn) ? string.Empty : $"""
                <publish-info>
                  <isbn>{isbn}</isbn>
                </publish-info>
                """)}}
              </description>
              <body>
                <section><p>Body</p></section>
              </body>
            </FictionBook>
            """);
        return sourcePath;
    }

    private static void TryDeleteDirectory(string path)
    {
        try
        {
            Directory.Delete(path, recursive: true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private sealed class FakeExportSelectionService(string exportPath) : Desktop.IPathSelectionService
    {
        public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken) =>
            Task.FromResult<IReadOnlyList<string>>([]);

        public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken) =>
            Task.FromResult<string?>(null);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken) =>
            Task.FromResult<string?>(null);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken) =>
            Task.FromResult<string?>(exportPath);
    }
}

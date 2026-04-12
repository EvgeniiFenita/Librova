using System;

namespace Librova.UI.LibraryCatalog;

internal enum LibraryCatalogErrorCodeModel
{
    Validation,
    UnsupportedFormat,
    DuplicateRejected,
    ParserFailure,
    ConverterUnavailable,
    ConverterFailed,
    StorageFailure,
    DatabaseFailure,
    Cancellation,
    IntegrityIssue,
    NotFound
}

internal sealed class LibraryCatalogDomainErrorModel
{
    public LibraryCatalogErrorCodeModel Code { get; init; }
    public string Message { get; init; } = string.Empty;
}

internal sealed class LibraryCatalogDomainException : InvalidOperationException
{
    public LibraryCatalogDomainException(LibraryCatalogDomainErrorModel error)
        : base(error.Message)
    {
        Error = error;
    }

    public LibraryCatalogDomainErrorModel Error { get; }
}

using System;

namespace Librova.UI.ImportJobs;

internal sealed class ImportJobDomainException : InvalidOperationException
{
    public ImportJobDomainException(DomainErrorModel error)
        : base(error.Message)
    {
        Error = error;
    }

    public DomainErrorModel Error { get; }
}

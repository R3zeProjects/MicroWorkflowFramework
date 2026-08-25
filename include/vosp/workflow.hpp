#pragma once

/** @file workflow.hpp Public entry point for MicroWorkflowFramework. */

#include <vosp/workflow/runner.hpp>
#include <vosp/workflow/types.hpp>
#include <vosp/workflow/version.hpp>
#include <vosp/workflow/workflow.hpp>

namespace vosp
{
using workflow::Capabilities;
using workflow::capabilities;
using workflow::ProcessResult;
using workflow::ProcessSpec;
using workflow::ResourceLimits;
using workflow::Step;
using workflow::StepResult;
using workflow::StopReason;
using workflow::WorkflowPolicy;
using workflow::WorkflowReport;

template <workflow::WorkflowErrorModel Model> using ProcessRunner = workflow::Runner<Model>;

template <workflow::WorkflowErrorModel Model> using ProcessWorkflow = workflow::Workflow<Model>;
} // namespace vosp

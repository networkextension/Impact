//
//  ImpactCrashTests.swift
//  ImpactTests
//
//  Created by Matt Massicotte on 2019-09-17.
//  Copyright © 2019 Chime Systems Inc. All rights reserved.
//

import XCTest

class ImpactCrashTests: XCTestCase {
    lazy var testAppURL: URL = {
        let bundleURL = Bundle(for: type(of: self)).bundleURL
        let buildDirectoryURL = bundleURL.deletingLastPathComponent()

        return buildDirectoryURL.appendingPathComponent("ImpactTestMac.app")
    }()

    lazy var outputDirectory: URL = {
        return URL(fileURLWithPath: NSTemporaryDirectory(), isDirectory: true)
    }()

    func launchAppAndExecute(crash name: String) throws -> URL {
        let tempURL = outputDirectory.appendingPathComponent(UUID().uuidString, isDirectory: false)

        Swift.print("setting: \(tempURL.path)")
        let args = ["-output_path", tempURL.path, "-run", name, "-suppressReportCrash", "YES"]

        let app = try NSWorkspace.shared.launchApplication(at: testAppURL,
                                                           options: [.withoutActivation],
                                                           configuration: [.arguments: args])

        let launchExpectation = keyValueObservingExpectation(for: app, keyPath: "isFinishedLaunching", expectedValue: true)
        let terminationExpectation = keyValueObservingExpectation(for: app, keyPath: "isTerminated", expectedValue: true)

        wait(for: [launchExpectation, terminationExpectation], timeout: 5.0, enforceOrder: true)

        return tempURL
    }

    func testCallAbort() throws {
        let url = try launchAppAndExecute(crash: "invokeAbort")

        let contents = try Data(contentsOf: url)
        let stringContents = String(data: contents, encoding: .utf8)

        guard let lines = stringContents?.components(separatedBy: "\n") else {
            XCTFail()
            return
        }

        XCTAssertEqual(lines.count, 4)
        XCTAssertEqual(lines[2], "hello from the signal handler: 0x0")

        try? FileManager.default.removeItem(at: url)
    }

    func testNullDereference() throws {
        let url = try launchAppAndExecute(crash: "nullDereference")

        let contents = try Data(contentsOf: url)
        let stringContents = String(data: contents, encoding: .utf8)

        guard let lines = stringContents?.components(separatedBy: "\n") else {
            XCTFail()
            return
        }

        XCTAssertEqual(lines.count, 11)
        XCTAssertEqual(lines[9], "hello from the signal handler: 0x0")

        try? FileManager.default.removeItem(at: url)
    }
}